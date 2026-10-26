#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "device_modes.h"

// Escape network names before JSON output. The dashboard renders them as text.
inline String jsonQuote(const String &value) {
  String out = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    uint8_t c = uint8_t(value[i]);
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 32) { char escape[7]; snprintf(escape, sizeof(escape), "\\u%04x", c); out += escape; }
    else out += char(c);
  }
  return out + "\"";
}
inline const char *wifiChoiceName(WifiChoice mode) {
  return mode == WifiChoice::Both ? "both" : mode == WifiChoice::Join ? "join" : "host";
}
class MonitorNetwork {
public:
  NetworkSettings settings = {1, WifiChoice::Host, {}, {}};
  bool hotspot = false;
  String hostname;
  void begin(const String &password) {
    apPassword = password;
    char name[32];
    snprintf(name, sizeof(name), "soil-monitor-%06x", unsigned(ESP.getEfuseMac() & 0xffffff));
    hostname = name;
    WiFi.persistent(false); // credentials belong only in our Preferences blob
    WiFi.setHostname(hostname.c_str());
    apply();
    mdns = MDNS.begin(hostname.c_str());
    if (mdns) MDNS.addService("http", "tcp", 80);
  }
  void scheduleApply() { pending = true; pendingSince = millis(); }
  bool isPending() const { return pending; }
  bool joined() const { return settings.mode != WifiChoice::Host && WiFi.status() == WL_CONNECTED; }
  bool connecting() const {
    return settings.mode != WifiChoice::Host && !joined() &&
           uint32_t(millis() - attemptSince) < JOIN_ATTEMPT_MS;
  }
  void tick() {
    uint32_t now = millis();
    if (pending && uint32_t(now - pendingSince) >= 1000) { pending = false; apply(); }
    bool connected = joined();
    if (connected && !wasJoined) { connectedSince = now; avoidSubnetCollision(); }
    wasJoined = connected;
    bool wanted = keepHotspot(settings.mode, connected, uint32_t(now - connectedSince));
    if (wanted && !hotspot) startHotspot();
    if (!wanted && hotspot && WiFi.mode(WIFI_STA)) hotspot = false;
    if (settings.mode != WifiChoice::Host && !connected &&
        uint32_t(now - attemptSince) >= JOIN_RETRY_MS && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
      attemptSince = now;
      WiFi.begin(settings.ssid, settings.password[0] ? settings.password : nullptr);
    }
  }
  String statusJson() const {
    bool connected = joined();
    uint32_t elapsed = uint32_t(millis() - connectedSince);
    uint32_t remaining = settings.mode == WifiChoice::Join && connected && hotspot && elapsed < JOIN_AP_GRACE_MS
                           ? (JOIN_AP_GRACE_MS - elapsed + 999) / 1000 : 0;
    String s = "{\"mode\":" + jsonQuote(wifiChoiceName(settings.mode));
    s += ",\"ssid\":" + jsonQuote(settings.ssid);
    s += ",\"passwordSaved\":" + String(settings.password[0] ? "true" : "false");
    s += ",\"joined\":" + String(connected ? "true" : "false");
    s += ",\"connecting\":" + String(connecting() ? "true" : "false");
    s += ",\"hotspot\":" + String(hotspot ? "true" : "false");
    s += ",\"apIp\":" + jsonQuote(apIp.toString());
    s += ",\"stationIp\":" + jsonQuote(connected ? WiFi.localIP().toString() : String(""));
    s += ",\"hostname\":" + jsonQuote(mdns ? hostname + ".local" : String(""));
    s += ",\"hotspotClosesIn\":" + String(remaining);
    s += ",\"pending\":" + String(pending ? "true" : "false") + "}";
    return s;
  }
  // Scans temporarily enable the station interface, without disabling the AP.
  int startScan() {
    if (pending || connecting()) return -3;
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return WIFI_SCAN_RUNNING;
    WiFi.scanDelete();
    WiFi.mode(hotspot ? WIFI_AP_STA : WIFI_STA);
    return WiFi.scanNetworks(true, false, false, 120);
  }
private:
  String apPassword;
  IPAddress apIp = IPAddress(192, 168, 4, 1);
  bool pending = false, wasJoined = false, mdns = false;
  uint32_t pendingSince = 0, attemptSince = 0, connectedSince = 0;
  void startHotspot() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
    hotspot = WiFi.softAP("Soil Monitor", apPassword.c_str(), 1, false, 2);
  }
  void apply() {
    // Retain AP service while changing station credentials; no reboot/log reset.
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP_STA);
    if (!hotspot) startHotspot();
    wasJoined = false;
    attemptSince = millis();
    if (settings.mode != WifiChoice::Host) {
      WiFi.setAutoReconnect(true);
      WiFi.begin(settings.ssid, settings.password[0] ? settings.password : nullptr);
    } else {
      WiFi.setAutoReconnect(false);
    }
  }
  void avoidSubnetCollision() {
    // Select an AP /24 that does not overlap the joined network's subnet.
    uint32_t mask = networkOrder(WiFi.subnetMask());
    uint32_t station = networkOrder(WiFi.localIP());
    uint32_t low = station & mask, high = low | ~mask;
    IPAddress candidates[] = {IPAddress(192,168,4,1), IPAddress(192,168,5,1),
                              IPAddress(172,31,254,1), IPAddress(10,254,254,1)};
    for (const auto &candidate : candidates) {
      uint32_t apLow = networkOrder(candidate) & 0xffffff00;
      if (apLow <= high && apLow + 255 >= low) continue;
      if (candidate != apIp) {
        apIp = candidate;
        if (hotspot) WiFi.softAPConfig(apIp, apIp, IPAddress(255,255,255,0));
      }
      break;
    }
  }
  static uint32_t networkOrder(const IPAddress &ip) {
    return uint32_t(ip[0]) << 24 | uint32_t(ip[1]) << 16 | uint32_t(ip[2]) << 8 | ip[3];
  }
};
