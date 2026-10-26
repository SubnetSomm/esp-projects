#include "../src/networking.h"
#include <cassert>
uint32_t mockMillis=0;
MockEsp ESP; MockWifi WiFi; MockMdns MDNS;
int main(){
 MonitorNetwork n;
 n.begin("local-hotspot-pass");
 assert(n.hotspot && !n.joined() && !n.connecting());
 assert(!WiFi.persistence && WiFi.attempts==0);
 // Joining starts only after the save response grace and preserves the AP.
 n.settings.mode=WifiChoice::Both;
 std::strcpy(n.settings.ssid,"test-network");
 std::strcpy(n.settings.password,"example-secret");
 n.scheduleApply();mockMillis=999;n.tick();assert(WiFi.attempts==0);
 mockMillis=1000;n.tick();assert(WiFi.attempts==1 && n.hotspot && n.connecting());
 assert(WiFi.lastPassword=="example-secret");
 assert(n.statusJson().find("example-secret")==String::npos);
 assert(n.statusJson().find("local-hotspot-pass")==String::npos);
 // A failed attempt never drops fallback access; reconnects are bounded.
 mockMillis=21000;n.tick();assert(!n.connecting() && n.hotspot && WiFi.attempts==1);
 mockMillis=61000;n.tick();assert(WiFi.attempts==2 && n.hotspot);
 // Both mode retains access even after a successful join beyond two minutes.
 WiFi.wifiStatus=WL_CONNECTED;mockMillis=62000;n.tick();
 mockMillis=182000;n.tick();assert(n.hotspot && n.joined());
 // Join-only disables the actual AP radio after its grace, not only its SSID.
 n.settings.mode=WifiChoice::Join;n.scheduleApply();mockMillis=183000;n.tick();
 WiFi.wifiStatus=WL_CONNECTED;mockMillis=184000;n.tick();
 mockMillis=303999;n.tick();assert(n.hotspot);
 mockMillis=304000;n.tick();assert(!n.hotspot && WiFi.radioMode==WIFI_STA);
 assert(n.statusJson().find("192.168.1.80")!=String::npos);
 // Losing the network immediately restores a password-protected hotspot.
 WiFi.wifiStatus=0;mockMillis=305000;n.tick();assert(n.hotspot && WiFi.radioMode==WIFI_AP_STA);
 // Handle a router using the hotspot's default subnet.
 WiFi.address=IPAddress(192,168,4,80);WiFi.wifiStatus=WL_CONNECTED;mockMillis=306000;n.tick();
 assert(WiFi.apAddress.toString()=="192.168.5.1");
 // Host mode stops station connection attempts but still supports scanning.
 n.settings.mode=WifiChoice::Host;n.scheduleApply();mockMillis=307000;n.tick();
 int attempts=WiFi.attempts;mockMillis=500000;n.tick();
 assert(n.hotspot && !n.joined() && !n.connecting() && WiFi.attempts==attempts);
 assert(n.startScan()==WIFI_SCAN_RUNNING && n.hotspot);
 // Network names are JSON escaped and never interpreted as HTML.
 assert(jsonQuote("quote\"\\\n")=="\"quote\\\"\\\\\\u000a\"");
}
