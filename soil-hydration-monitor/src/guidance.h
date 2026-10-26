#pragma once
#include <stdint.h>

// Provisional references from one planted-soil watering cycle. This scale is
// relative to the watering point, not volumetric water content or bone-dry soil.
constexpr uint16_t SOIL_NEEDS_WATER_RAW = 2959;
constexpr uint16_t SOIL_SETTLED_WET_RAW = 2574;
constexpr uint8_t GUIDANCE_CONFIRM_SAMPLES = 3;
constexpr uint32_t GUIDANCE_CONFIRM_SECONDS = 30;

inline bool validSoilCalibration(uint16_t dry, uint16_t wet) {
  return dry <= 4095 && wet <= 4095 &&
         (dry > wet ? dry - wet : wet - dry) >= 100;
}
inline bool validSoilRaw(uint16_t raw) { return raw > 100 && raw < 4090; }
inline float relativeSoilMoisture(uint16_t raw, uint16_t dry, uint16_t wet) {
  if (!validSoilCalibration(dry, wet)) return 0;
  float pct = 100.0f * (float(dry) - raw) / (float(dry) - wet);
  return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}
struct GuidanceState {
  bool needsWater;
  uint8_t dryCount, wetCount;
  uint32_t drySince, wetSince;
};
inline void resetGuidance(GuidanceState &s, bool latched = false) {
  s = {latched, 0, 0, 0, 0};
}
inline void observeSoil(GuidanceState &s, uint16_t raw, uint16_t dry,
                        uint16_t wet, uint32_t seconds) {
  if (!validSoilRaw(raw) || !validSoilCalibration(dry, wet)) {
    // A faulty reading breaks consecutive evidence, but cannot clear an alert.
    s.dryCount = s.wetCount = 0;
    return;
  }
  float pct = relativeSoilMoisture(raw, dry, wet);
  if (pct <= 0) {
    s.wetCount = 0;
    if (!s.dryCount) s.drySince = seconds;
    if (s.dryCount < GUIDANCE_CONFIRM_SAMPLES) ++s.dryCount;
    if (s.dryCount >= GUIDANCE_CONFIRM_SAMPLES &&
        uint32_t(seconds - s.drySince) >= GUIDANCE_CONFIRM_SECONDS)
      s.needsWater = true;
  } else if (pct >= 20) {
    s.dryCount = 0;
    if (!s.wetCount) s.wetSince = seconds;
    if (s.wetCount < GUIDANCE_CONFIRM_SAMPLES) ++s.wetCount;
    if (s.wetCount >= GUIDANCE_CONFIRM_SAMPLES &&
        uint32_t(seconds - s.wetSince) >= GUIDANCE_CONFIRM_SECONDS)
      s.needsWater = false;
  } else {
    s.dryCount = s.wetCount = 0;
  }
}
inline const char *soilAdvice(const GuidanceState &s, uint16_t raw,
                              uint16_t dry, uint16_t wet) {
  if (!validSoilRaw(raw) || !validSoilCalibration(dry, wet)) return "check_sensor";
  if (s.needsWater) return "water_needed";
  float pct = relativeSoilMoisture(raw, dry, wet);
  if (pct <= 0) return "confirm_dry";
  return pct <= 15 ? "water_soon" : "moisture_okay";
}
