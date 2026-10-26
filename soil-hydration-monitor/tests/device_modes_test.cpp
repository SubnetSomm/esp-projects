#include "../src/device_modes.h"
#include <cassert>
#include <cstring>
#include <initializer_list>
int main() {
 NetworkSettings s = {1, WifiChoice::Host, {}, {}};
 assert(validNetworkSettings(s));
 s.mode = WifiChoice::Join; assert(!validNetworkSettings(s));
 std::strcpy(s.ssid, "Example network"); assert(validNetworkSettings(s));
 std::strcpy(s.password, "short"); assert(!validNetworkSettings(s));
 std::strcpy(s.password, "test-passphrase"); assert(validNetworkSettings(s));
 s.mode = WifiChoice::Both; assert(validNetworkSettings(s));
 s.mode = WifiChoice(3); assert(!validNetworkSettings(s));
 s.mode = WifiChoice::Both; s.version = 2; assert(!validNetworkSettings(s));
 s.version = 1; std::memset(s.ssid, 's', sizeof(s.ssid)); assert(!validNetworkSettings(s));
 s.ssid[32] = 0; assert(validNetworkSettings(s));
 std::memset(s.password, 'p', sizeof(s.password)); assert(!validNetworkSettings(s));
 s.password[63] = 0; assert(validNetworkSettings(s));
 for (WifiChoice mode : {WifiChoice::Host, WifiChoice::Join, WifiChoice::Both}) {
   assert(keepHotspot(mode, false, 0));
   assert(keepHotspot(mode, false, 600000)); // network loss always restores access
   assert(keepHotspot(mode, true, JOIN_AP_GRACE_MS-1));
 }
 assert(!keepHotspot(WifiChoice::Join, true, JOIN_AP_GRACE_MS));
 assert(keepHotspot(WifiChoice::Both, true, JOIN_AP_GRACE_MS));
 assert(keepHotspot(WifiChoice::Host, false, JOIN_AP_GRACE_MS));
 // Count pulses over five seconds to distinguish user-visible states.
 auto pulses=[](LedSignal signal) {int n=0;bool prior=false;for(uint32_t t=0;t<5000;++t){bool on=ledOn(signal,t);if(on&&!prior)++n;prior=on;}return n;};
 assert(pulses(LedSignal::HotspotMonitor)==1);
 assert(pulses(LedSignal::JoinedMonitor)==2);
 assert(pulses(LedSignal::WaterNeeded)==3);
 assert(pulses(LedSignal::Watering)==5);
 assert(pulses(LedSignal::Connecting)==25);
 assert(pulses(LedSignal::Sleeping)==0);
 assert(ledOn(LedSignal::SleepReady,0) && ledOn(LedSignal::SleepReady,5001));
 assert(ledOn(LedSignal::Fault,0) && !ledOn(LedSignal::Fault,700));
 assert(!ledOn(LedSignal::HotspotMonitor,100));
 assert(ledOn(LedSignal::HotspotMonitor,5000));
}
