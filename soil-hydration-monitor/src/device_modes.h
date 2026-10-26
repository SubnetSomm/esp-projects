#pragma once
#include <stdint.h>
#include <stddef.h>

enum class WifiChoice : uint8_t { Host = 0, Join = 1, Both = 2 };
constexpr uint32_t JOIN_AP_GRACE_MS = 120000;
constexpr uint32_t JOIN_ATTEMPT_MS = 20000;
constexpr uint32_t JOIN_RETRY_MS = 60000;
struct NetworkSettings {
  uint8_t version;
  WifiChoice mode;
  char ssid[33];
  char password[64];
};
inline bool validNetworkSettings(const NetworkSettings &s) {
  if (s.version != 1 || uint8_t(s.mode) > uint8_t(WifiChoice::Both)) return false;
  size_t n = 0, p = 0;
  while (n < sizeof(s.ssid) && s.ssid[n]) ++n;
  while (p < sizeof(s.password) && s.password[p]) ++p;
  if (n == sizeof(s.ssid) || p == sizeof(s.password)) return false;
  if (s.mode != WifiChoice::Host && n == 0) return false;
  return p == 0 || p >= 8;
}
inline bool keepHotspot(WifiChoice mode, bool joined, uint32_t connectedMs) {
  return mode != WifiChoice::Join || !joined || connectedMs < JOIN_AP_GRACE_MS;
}
enum class LedSignal : uint8_t {
  Sleeping, SleepReady, Fault, Connecting, Watering, WaterNeeded, JoinedMonitor, HotspotMonitor
};
inline const char *ledSignalName(LedSignal s) {
  switch (s) {
    case LedSignal::Sleeping: return "off";
    case LedSignal::SleepReady: return "release_to_sleep";
    case LedSignal::Fault: return "check_device";
    case LedSignal::Connecting: return "joining_wifi";
    case LedSignal::Watering: return "watering";
    case LedSignal::WaterNeeded: return "water_needed";
    case LedSignal::JoinedMonitor: return "joined_monitor";
    default: return "hotspot_monitor";
  }
}
// All patterns are nonblocking. Brief monitoring pulses limit LED power use.
inline bool ledOn(LedSignal signal, uint32_t now) {
  uint32_t phase = now % 5000;
  switch (signal) {
    case LedSignal::Sleeping: return false;
    case LedSignal::SleepReady: return true;
    case LedSignal::Fault: return now % 2000 < 700;
    case LedSignal::Connecting: return now % 200 < 100;
    case LedSignal::Watering: return now % 1000 < 150;
    case LedSignal::WaterNeeded: return phase < 100 || (phase >= 300 && phase < 400) || (phase >= 600 && phase < 700);
    case LedSignal::JoinedMonitor: return phase < 100 || (phase >= 300 && phase < 400);
    default: return phase < 100;
  }
}
