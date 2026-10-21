#pragma once
#include <stdint.h>

inline bool validBatteryMv(uint32_t mv) { return mv >= 2500 && mv <= 4500; }
// Preserve the original 16-byte record format: the formerly unused byte now
// stores battery voltage in 20 mV increments. Zero means unknown/legacy data.
inline uint8_t encodeBatteryMv(uint32_t mv) {
  return validBatteryMv(mv) ? uint8_t((mv + 10) / 20) : 0;
}
inline uint16_t decodeBatteryMv(uint8_t code) { return uint16_t(code) * 20; }
inline float batteryPercent(uint32_t mv) {
  // Generic single-cell LiPo estimate, not a fuel gauge or charge indicator.
  constexpr uint16_t volts[] = {3000,3300,3500,3600,3700,3800,3900,4000,4100,4200};
  constexpr uint8_t pct[] = {0,5,10,20,40,60,75,85,95,100};
  if (mv <= volts[0]) return 0;
  for (unsigned i=1;i<sizeof(volts)/sizeof(volts[0]);++i)
    if (mv <= volts[i]) return pct[i-1] + float(mv-volts[i-1])*(pct[i]-pct[i-1])/(volts[i]-volts[i-1]);
  return 100;
}
