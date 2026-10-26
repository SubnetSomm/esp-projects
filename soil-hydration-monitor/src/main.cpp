#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <sys/time.h>
#include "dashboard.h"
#include "battery.h"
#include "guidance.h"
#include "networking.h"
RTC_DATA_ATTR GuidanceState guidance = {};

// Soil probe, battery divider, and active-low momentary button.
constexpr int LED_PIN = 21; // D6/TX -> 1K resistor -> LED anode; cathode -> GND
constexpr int BUTTON_PIN = 4; // D2/A2, external 10K pull-up to 3V3; switch to GND
RTC_DATA_ATTR uint32_t retainedBoot = 0;
RTC_DATA_ATTR int64_t sessionOriginUs = 0;
int64_t clockUs() { timeval t; gettimeofday(&t, nullptr); return int64_t(t.tv_sec)*1000000+t.tv_usec; }
uint32_t sessionSeconds() { return uint32_t((clockUs()-sessionOriginUs)/1000000); }
bool buttonArmed = false, buttonRaw = false, buttonStable = false, sleepRequested = false;
uint32_t buttonChanged = 0, buttonPressed = 0;
constexpr int SOIL_PIN = A0;
constexpr int BATTERY_PIN = A1; // GPIO3: BAT+ -> 100K -> A1 -> 100K -> GND
constexpr uint32_t MONITOR_MS = 15UL * 60 * 1000;
constexpr uint32_t WATER_MS = 5000;
constexpr uint32_t WATER_TIMEOUT_MS = 10UL * 60 * 1000;
constexpr uint32_t FILE_RECORDS = 4096;
constexpr uint32_t VALID_EPOCH = 1700000000;
const char *CURRENT = "/current.bin", *PREVIOUS = "/previous.bin";
struct Record {
  uint32_t sequence, boot, seconds;
  uint16_t raw;
  uint8_t watering, battery20mV;
};
static_assert(sizeof(Record) == 16, "Record format changed");
struct Anchor { uint32_t boot, seconds, epoch; };
Preferences prefs;
WebServer server(80);
Anchor anchors[32] = {};
uint32_t bootId, sequence = 0, lastSample = 0, wateringSince = 0;
uint32_t currentCount = 0, anchorSlot = 0;
uint16_t latestRaw = 0, dryRaw = SOIL_NEEDS_WATER_RAW, wetRaw = SOIL_SETTLED_WET_RAW;
bool persistedNeedsWater = false;
uint16_t latestBatteryMv = 0;
bool watering = false, storageReady = false, writeHealthy = true;
String apPassword;
String csrf;
MonitorNetwork network;
LedSignal currentLedSignal() {
  if (sleepRequested) return LedSignal::SleepReady;
  if (!storageReady || !writeHealthy || !validSoilRaw(latestRaw)) return LedSignal::Fault;
  if (network.connecting()) return LedSignal::Connecting;
  if (watering) return LedSignal::Watering;
  if (guidance.needsWater) return LedSignal::WaterNeeded;
  return network.joined() ? LedSignal::JoinedMonitor : LedSignal::HotspotMonitor;
}
void updateLed() { digitalWrite(LED_PIN, ledOn(currentLedSignal(), millis()) ? HIGH : LOW); }


float moisture(uint16_t raw) {
  return relativeSoilMoisture(raw, dryRaw, wetRaw);
}
uint32_t recordEpoch(const Record &r) {
  for (const auto &a : anchors) if (a.boot == r.boot && a.epoch >= VALID_EPOCH) {
    int64_t t = int64_t(a.epoch) + int64_t(r.seconds) - a.seconds;
    if (t >= VALID_EPOCH && t <= UINT32_MAX) return uint32_t(t);
  }
  return 0;
}
uint32_t countFile(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  // Never append after a torn record: keep existing full records exportable
  // and surface the storage fault instead of shifting subsequent records.
  if (f.size() % sizeof(Record)) writeHealthy = false;
  uint32_t n = f.size() / sizeof(Record);
  if (n) {
    Record r;
    f.seek((n - 1) * sizeof(r));
    if (f.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) == sizeof(r))
      sequence = max(sequence, r.sequence);
  }
  f.close();
  return n;
}
void sample() {
  uint32_t total = 0;
  for (int i = 0; i < 16; ++i) { total += analogRead(SOIL_PIN); delay(2); }
  latestRaw = total / 16;
  // Discard initial conversions after changing ADC channel; average calibrated
  // millivolts. The high-resistance divider should be checked against a meter.
  analogReadMilliVolts(BATTERY_PIN); delay(5);
  analogReadMilliVolts(BATTERY_PIN); delay(5);
  uint32_t batteryTotal = 0;
  for (int i=0;i<32;++i) { batteryTotal += analogReadMilliVolts(BATTERY_PIN); delay(3); }
  latestBatteryMv = calibrateBatteryMv((batteryTotal * 2 + 16) / 32);
  lastSample = millis();
  observeSoil(guidance, latestRaw, dryRaw, wetRaw, sessionSeconds());
  if (persistedNeedsWater != guidance.needsWater) {
    if (prefs.putBool("needsWater", guidance.needsWater)) persistedNeedsWater = guidance.needsWater;
  }
  Record r{++sequence, bootId, sessionSeconds(), latestRaw, uint8_t(watering), encodeBatteryMv(latestBatteryMv)};
  if (storageReady) {
    if (currentCount >= FILE_RECORDS) {
      if (LittleFS.exists(PREVIOUS) && !LittleFS.remove(PREVIOUS)) writeHealthy = false;
      if (writeHealthy && LittleFS.rename(CURRENT, PREVIOUS)) currentCount = 0;
      else writeHealthy = false;
    }
    if (writeHealthy) {
      File f = LittleFS.open(CURRENT, "a");
      if (!f || f.write(reinterpret_cast<uint8_t *>(&r), sizeof(r)) != sizeof(r)) writeHealthy = false;
      else ++currentCount;
      if (f) { f.flush(); f.close(); }
    }
  }
  Serial.printf("raw=%u moisture=%.1f mode=%s history=%s\n", latestRaw, moisture(latestRaw), watering ? "watering" : "monitor", storageReady && writeHealthy ? "ok" : "unavailable");
  Serial.printf("battery=%u mV valid=%s estimate=%.0f%%\n",latestBatteryMv,validBatteryMv(latestBatteryMv)?"yes":"no",batteryPercent(latestBatteryMv));
}
// No open log handles survive sample(); flush and close precede sleep.
void enterSleep() {
  if (!storageReady || !writeHealthy) {
    Serial.println("Sleep cancelled: history storage unavailable.");
    sleepRequested = false;
    return;
  }
  if (digitalRead(BUTTON_PIN) == LOW) return; // must release before arming low-level wake
  esp_err_t gpioResult = esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_err_t timerResult = esp_sleep_enable_timer_wakeup(uint64_t(MONITOR_MS)*1000);
  if (gpioResult != ESP_OK || timerResult != ESP_OK) {
    Serial.println("Sleep cancelled: wake source configuration failed.");
    sleepRequested = false;
    return;
  }
  Serial.println("Deep sleep: 15-minute logging; press D2 button to open hotspot.");
  Serial.flush();
  server.stop();
  WiFi.mode(WIFI_OFF);
  digitalWrite(LED_PIN, LOW);
  LittleFS.end();
  esp_deep_sleep_start();
}
void handleButton() {
  bool down = digitalRead(BUTTON_PIN) == LOW;
  uint32_t now = millis();
  if (down != buttonRaw) { buttonRaw = down; buttonChanged = now; }
  if (uint32_t(now-buttonChanged) < 40) return;
  if (!buttonArmed) {
    if (!down) { buttonArmed = true; buttonStable = false; }
    return; // ignore the press that woke the device until released
  }
  if (down != buttonStable) {
    buttonStable = down;
    if (down) buttonPressed = now;
    else if (sleepRequested) {
      watering = false;
      sample();
      enterSleep();
    }
  }
  if (buttonStable && !sleepRequested && uint32_t(now-buttonPressed) >= 2000) {
    sleepRequested = true;
    Serial.println("Sleep requested: release button to save and sleep.");
  }
}
void setMode(bool on) {
  watering = on;
  if (on) wateringSince = millis();
  sample();
}
bool authorize() {
  if (server.header("X-Soil-Token") != csrf) { server.send(403, "application/json", "{\"error\":\"Reload the dashboard and try again\"}"); return false; }
  return true;
}
void status() {
  Record now{sequence, bootId, sessionSeconds(), latestRaw, uint8_t(watering), 0};
  uint32_t epoch = recordEpoch(now);
  String s = "{\"raw\":" + String(latestRaw) + ",\"moisture\":" + String(moisture(latestRaw), 1);
  s += ",\"mode\":\"" + String(watering ? "watering" : "monitor") + "\",\"boot\":" + String(bootId);
  s += ",\"epoch\":" + String(epoch) + ",\"seconds\":" + String(now.seconds);
  s += ",\"sampleAge\":" + String(uint32_t(millis()-lastSample)/1000);
  s += ",\"nextSample\":" + String(max(0L, long((watering ? WATER_MS : MONITOR_MS) - min(uint32_t(millis()-lastSample), watering ? WATER_MS : MONITOR_MS)))/1000);
  s += ",\"wateringRemaining\":" + String(watering ? (WATER_TIMEOUT_MS-min(uint32_t(millis()-wateringSince), WATER_TIMEOUT_MS))/1000 : 0);
  s += ",\"dry\":" + String(dryRaw) + ",\"wet\":" + String(wetRaw);
  s += ",\"soilValid\":" + String(validSoilRaw(latestRaw) ? "true" : "false");
  s += ",\"advice\":\"" + String(soilAdvice(guidance, latestRaw, dryRaw, wetRaw)) + "\"";
  s += ",\"provisionalCalibration\":" + String(dryRaw == SOIL_NEEDS_WATER_RAW && wetRaw == SOIL_SETTLED_WET_RAW ? "true" : "false");
  s += ",\"batteryMv\":" + (validBatteryMv(latestBatteryMv) ? String(latestBatteryMv) : String("null"));
  s += ",\"batteryPercent\":" + (validBatteryMv(latestBatteryMv) ? String(batteryPercent(latestBatteryMv),0) : String("null"));
  s += ",\"storage\":" + String(storageReady && writeHealthy ? "true" : "false");
  s += ",\"records\":" + String(storageReady ? currentCount + countFile(PREVIOUS) : 0);
  s += ",\"wifi\":" + network.statusJson();
  s += ",\"led\":\"" + String(ledSignalName(currentLedSignal())) + "\"";
  s += ",\"token\":\"" + csrf + "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}
void sendHistory(bool csv) {
  server.sendHeader("Cache-Control", "no-store");
  if (csv) server.sendHeader("Content-Disposition", "attachment; filename=soil-history.csv");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, csv ? "text/csv" : "application/json", "");
  server.sendContent(csv ? "sequence,boot,uptime_seconds,unix_seconds,raw,relative_moisture,mode,battery_voltage,battery_estimated_percent\n" : "[");
  bool first = true;
  if (storageReady) for (const char *path : {PREVIOUS, CURRENT}) {
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    Record r;
    String chunk; chunk.reserve(2048);
    while (f.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) == sizeof(r)) {
      uint32_t epoch = recordEpoch(r);
      uint16_t batteryMv=decodeBatteryMv(r.battery20mV);
      if (csv) {
        chunk += String(r.sequence) + "," + String(r.boot) + "," + String(r.seconds) + ",";
        if (epoch) chunk += String(epoch);
        chunk += "," + String(r.raw) + "," + String(moisture(r.raw),1) + "," + (r.watering ? "watering" : "monitor");
        chunk += "," + (validBatteryMv(batteryMv)?String(batteryMv/1000.0f,2):String(""));
        chunk += "," + (validBatteryMv(batteryMv)?String(batteryPercent(batteryMv),0):String("")) + "\n";
      } else {
        if (!first) chunk += ",";
        chunk += "[" + String(r.sequence) + "," + String(r.boot) + "," + String(r.seconds) + "," + String(epoch) + "," + String(r.raw) + "," + String(moisture(r.raw),1) + "," + String(r.watering) + "," + (validBatteryMv(batteryMv)?String(batteryMv):String("null")) + "]";
      }
      first = false;
      if (chunk.length() >= 1500) { server.sendContent(chunk); chunk = ""; delay(1); }
    }
    f.close();
    if (chunk.length()) server.sendContent(chunk);
  }
  if (!csv) server.sendContent("]");
  server.sendContent("");
}
void wifiSettings() {
  if (!authorize()) return;
  if (network.isPending()) { server.send(409, "application/json", "{\"error\":\"Previous Wi-Fi settings are still applying\"}"); return; }
  String mode = server.arg("mode");
  if (mode != "host" && mode != "join" && mode != "both") {
    server.send(400, "application/json", "{\"error\":\"Choose Host, Join, or Host + Join\"}"); return;
  }
  NetworkSettings next = network.settings;
  next.mode = mode == "both" ? WifiChoice::Both : mode == "join" ? WifiChoice::Join : WifiChoice::Host;
  if (next.mode != WifiChoice::Host) {
    String ssid = server.arg("ssid"), password = server.arg("password");
    if (ssid.length() < 1 || ssid.length() > 32 || password.length() > 63) {
      server.send(400, "application/json", "{\"error\":\"Network name must be 1–32 bytes; password at most 63 bytes\"}"); return;
    }
    for (char c : ssid) if (!c) { server.send(400, "application/json", "{\"error\":\"Invalid network name\"}"); return; }
    for (char c : password) if (!c) { server.send(400, "application/json", "{\"error\":\"Invalid password\"}"); return; }
    bool open = server.arg("open") == "true";
    bool sameNetwork = ssid == String(network.settings.ssid);
    if (open) password = "";
    else if (!password.length() && sameNetwork && network.settings.password[0]) password = network.settings.password;
    else if (password.length() < 8) {
      server.send(400, "application/json", "{\"error\":\"Enter an 8–63 byte Wi-Fi password or select Open network\"}"); return;
    }
    memset(next.ssid, 0, sizeof(next.ssid)); memset(next.password, 0, sizeof(next.password));
    ssid.toCharArray(next.ssid, sizeof(next.ssid)); password.toCharArray(next.password, sizeof(next.password));
  }
  if (!validNetworkSettings(next)) { server.send(400, "application/json", "{\"error\":\"Invalid Wi-Fi settings\"}"); return; }
  if (prefs.putBytes("wifiCfg", &next, sizeof(next)) != sizeof(next)) {
    server.send(500, "application/json", "{\"error\":\"Could not save Wi-Fi settings\"}"); return;
  }
  network.settings = next;
  network.scheduleApply(); // return response before changing the radio
  status();
}
void wifiScanStart() {
  if (!authorize()) return;
  int result = network.startScan();
  if (result == -3) { server.send(409, "application/json", "{\"error\":\"Wait for the Wi-Fi connection attempt to finish, then scan again\"}"); return; }
  if (result == WIFI_SCAN_FAILED) { server.send(503, "application/json", "{\"error\":\"Scan failed; enter the network name manually\"}"); return; }
  server.send(202, "application/json", "{\"scanning\":true}");
}
void wifiScanResults() {
  int count = WiFi.scanComplete();
  String out = "{\"scanning\":" + String(count == WIFI_SCAN_RUNNING ? "true" : "false");
  out += ",\"failed\":" + String(count == WIFI_SCAN_FAILED ? "true" : "false") + ",\"networks\":[";
  for (int i = 0; i < count && i < 40; ++i) {
    if (i) out += ",";
    out += "{\"ssid\":" + jsonQuote(WiFi.SSID(i)) + ",\"rssi\":" + String(WiFi.RSSI(i));
    out += ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out + "]}");
}
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  auto wakeCause = esp_sleep_get_wakeup_cause();
  bool sleepWake = wakeCause == ESP_SLEEP_WAKEUP_TIMER || wakeCause == ESP_SLEEP_WAKEUP_GPIO;
  if (!sleepWake || !retainedBoot) {
    retainedBoot = esp_random(); if (!retainedBoot) retainedBoot = 1;
    sessionOriginUs = clockUs();
  }
  buttonRaw = digitalRead(BUTTON_PIN) == LOW;
  buttonChanged = millis();
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
  prefs.begin("soil-local", false);
  dryRaw = prefs.getUShort("dry", SOIL_NEEDS_WATER_RAW);
  wetRaw = prefs.getUShort("wet", SOIL_SETTLED_WET_RAW);
  // Only replace the original air/water defaults; preserve custom calibration.
  if (!prefs.getBool("soilCycle1", false)) {
    if (dryRaw == 3039 && wetRaw == 1370) {
      prefs.putUShort("priorDry", dryRaw); prefs.putUShort("priorWet", wetRaw);
      dryRaw = SOIL_NEEDS_WATER_RAW; wetRaw = SOIL_SETTLED_WET_RAW;
      prefs.putUShort("dry", dryRaw); prefs.putUShort("wet", wetRaw);
    }
    prefs.putBool("soilCycle1", true);
  }
  if (!validSoilCalibration(dryRaw, wetRaw)) {
    dryRaw = SOIL_NEEDS_WATER_RAW; wetRaw = SOIL_SETTLED_WET_RAW;
  }
  persistedNeedsWater = prefs.getBool("needsWater", false);
  if (!sleepWake) resetGuidance(guidance, persistedNeedsWater);
  if (prefs.getBytesLength("anchors") == sizeof(anchors)) prefs.getBytes("anchors",anchors,sizeof(anchors));
  anchorSlot = prefs.getUInt("anchorSlot",0)%32;
  bootId = retainedBoot;
  csrf = String(esp_random(),HEX) + String(esp_random(),HEX);
  apPassword = prefs.getString("apPass", "");
  if (apPassword.length() < 12) {
    apPassword = "Soil-" + String(esp_random(),HEX) + String(esp_random(),HEX);
    prefs.putString("apPass",apPassword);
  }
  // Only initialize an unformatted filesystem on the first standalone boot.
  storageReady = LittleFS.begin(false);
  if (!storageReady && !prefs.getBool("fsInit",false)) storageReady=LittleFS.begin(true);
  if (storageReady) { prefs.putBool("fsInit",true); countFile(PREVIOUS); currentCount=countFile(CURRENT); }
  // Timer wakes only log, unless someone is pressing the button.
  if (wakeCause == ESP_SLEEP_WAKEUP_TIMER && digitalRead(BUTTON_PIN) == HIGH) {
    delay(1000); // allow the continuously powered probe/ADC to settle
    sample();
    enterSleep();
  }
  NetworkSettings saved;
  if (prefs.getBytesLength("wifiCfg") == sizeof(saved)) {
    prefs.getBytes("wifiCfg", &saved, sizeof(saved));
    if (validNetworkSettings(saved)) network.settings = saved;
  }
  network.begin(apPassword);
  Serial.printf("\nWi-Fi: Soil Monitor\nPassword: %s\nDashboard: http://192.168.4.1\n",apPassword.c_str());
  const char *headers[]={"X-Soil-Token"}; server.collectHeaders(headers,1);
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html",DASHBOARD);});
  server.on("/api/status",HTTP_GET,status);
  server.on("/api/wifi",HTTP_GET,[]{server.sendHeader("Cache-Control", "no-store"); server.send(200,"application/json",network.statusJson());});
  server.on("/api/wifi",HTTP_POST,wifiSettings);
  server.on("/api/wifi/scan",HTTP_POST,wifiScanStart);
  server.on("/api/wifi/scan",HTTP_GET,wifiScanResults);
  server.on("/api/history",HTTP_GET,[]{sendHistory(false);});
  server.on("/history.csv",HTTP_GET,[]{sendHistory(true);});
  server.on("/api/time",HTTP_POST,[]{
    if (!authorize()) return;
    int64_t epoch=strtoll(server.arg("epoch").c_str(),nullptr,10);
    if(epoch<VALID_EPOCH || epoch>4102444800LL) {server.send(400,"application/json","{\"error\":\"Invalid phone time\"}");return;}
    uint32_t slot=32;
    for(uint32_t i=0;i<32;i++) if(anchors[i].boot==bootId) {slot=i;break;}
    if(slot==32) {slot=anchorSlot;anchorSlot=(anchorSlot+1)%32;}
    anchors[slot]={bootId,sessionSeconds(),uint32_t(epoch)};
    prefs.putBytes("anchors",anchors,sizeof(anchors));prefs.putUInt("anchorSlot",anchorSlot);
    server.send(200,"application/json","{\"ok\":true}");
  });
  server.on("/api/mode",HTTP_POST,[]{
    if(!authorize())return;
    String mode=server.arg("mode");
    if(mode!="monitor"&&mode!="watering") {server.send(400,"application/json","{\"error\":\"Invalid mode\"}");return;}
    setMode(mode=="watering");status();
  });
  server.on("/api/calibration",HTTP_POST,[]{
    if(!authorize())return;
    String d=server.arg("dry"),w=server.arg("wet");
    auto numeric=[](const String &s){if(!s.length())return false;for(char c:s)if(c<'0'||c>'9')return false;return true;};
    int dry=d.toInt(),wet=w.toInt();
    if(!numeric(d)||!numeric(w)||d.length()>4||w.length()>4||dry>4095||wet>4095||abs(dry-wet)<100) {server.send(400,"application/json","{\"error\":\"Use 0–4095 and separate endpoints by at least 100 counts\"}");return;}
    dryRaw=dry;wetRaw=wet;prefs.putUShort("dry",dryRaw);prefs.putUShort("wet",wetRaw);
    resetGuidance(guidance); persistedNeedsWater = false; prefs.putBool("needsWater", false);
    sample(); status();
  });
  server.onNotFound([]{server.send(404,"text/plain","Not found");});
  sample();server.begin();
  Serial.printf("Soil calibration: needs-water=%u settled-wet=%u; advice=%s\n", dryRaw, wetRaw, soilAdvice(guidance, latestRaw, dryRaw, wetRaw));
  Serial.printf("Button firmware ready: D2/GPIO4=%s; hold 2 seconds and release to sleep.\n", digitalRead(BUTTON_PIN)==LOW ? "pressed" : "released");
}
void loop() {
  server.handleClient();
  handleButton();
  network.tick();
  updateLed();
  if(Serial.available() && Serial.read()=='?') {
    Serial.printf("Wi-Fi: Soil Monitor\nPassword: %s\nDashboard: http://192.168.4.1\nHistory: %s (%u current records)\n",apPassword.c_str(),storageReady&&writeHealthy?"healthy":"unavailable",currentCount);
    Serial.printf("Battery: %u mV; valid=%s; estimate=%.0f%%\n",latestBatteryMv,validBatteryMv(latestBatteryMv)?"yes":"no",batteryPercent(latestBatteryMv));
    Serial.printf("Network: %s\nLED: %s\n", network.statusJson().c_str(), ledSignalName(currentLedSignal()));
  }
  if(watering && uint32_t(millis()-wateringSince)>=WATER_TIMEOUT_MS) setMode(false);
  if(uint32_t(millis()-lastSample)>=(watering?WATER_MS:MONITOR_MS)) sample();
  delay(2);
}
