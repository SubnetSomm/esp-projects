#include "../src/guidance.h"
#include <cassert>
#include <cstring>

int main() {
  const uint16_t dry = SOIL_NEEDS_WATER_RAW, wet = SOIL_SETTLED_WET_RAW;
  GuidanceState s{};
  assert(relativeSoilMoisture(dry, dry, wet) == 0);
  assert(relativeSoilMoisture(wet, dry, wet) == 100);
  assert(relativeSoilMoisture(3200, dry, wet) == 0);
  assert(relativeSoilMoisture(2000, dry, wet) == 100);
  assert(!validSoilCalibration(2500, 2500));
  assert(!validSoilCalibration(5000, 2000));
  assert(validSoilCalibration(wet, dry));
  assert(!validSoilRaw(100) && validSoilRaw(101));
  assert(!validSoilRaw(4090) && validSoilRaw(4089));
  // Several rapid samples cannot trigger an alert.
  for (uint32_t t = 0; t < 30; t += 5) observeSoil(s, dry, dry, wet, t);
  assert(!s.needsWater);
  assert(!strcmp(soilAdvice(s, dry, dry, wet), "confirm_dry"));
  observeSoil(s, dry, dry, wet, 30); assert(s.needsWater);
  // Hysteresis: a slight rise does not clear the alert.
  observeSoil(s, dry-20, dry, wet, 35); assert(s.needsWater);
  observeSoil(s, wet, dry, wet, 40);
  observeSoil(s, wet, dry, wet, 45);
  observeSoil(s, wet, dry, wet, 50); assert(s.needsWater);
  observeSoil(s, wet, dry, wet, 70); assert(!s.needsWater);
  assert(!strcmp(soilAdvice(s, wet, dry, wet), "moisture_okay"));
  assert(!strcmp(soilAdvice(s, dry-30, dry, wet), "water_soon"));
  assert(!strcmp(soilAdvice(s, 0, dry, wet), "check_sensor"));
  // Exact 20% clearing boundary; 15% warning boundary is between counts.
  resetGuidance(s, true);
  observeSoil(s, 2883, dry, wet, 0);
  observeSoil(s, 2883, dry, wet, 15);
  observeSoil(s, 2883, dry, wet, 30); assert(s.needsWater);
  observeSoil(s, 2882, dry, wet, 35);
  observeSoil(s, 2882, dry, wet, 50);
  observeSoil(s, 2882, dry, wet, 65); assert(!s.needsWater);
  assert(!strcmp(soilAdvice(s, 2902, dry, wet), "water_soon"));
  assert(!strcmp(soilAdvice(s, 2901, dry, wet), "moisture_okay"));
  // Monitoring evidence continues through sleep (state copied unchanged).
  resetGuidance(s); observeSoil(s, dry, dry, wet, 0);
  GuidanceState retained = s;
  observeSoil(retained, dry, dry, wet, 900); assert(!retained.needsWater);
  observeSoil(retained, dry, dry, wet, 1800); assert(retained.needsWater);
  // Invalid samples break consecutive evidence and do not clear a latch.
  observeSoil(retained, 0, dry, wet, 1805); assert(retained.needsWater);
  observeSoil(retained, wet, dry, wet, 1810);
  observeSoil(retained, 4095, dry, wet, 1815);
  observeSoil(retained, wet, dry, wet, 1820);
  observeSoil(retained, wet, dry, wet, 1850); assert(retained.needsWater);
  observeSoil(retained, wet, dry, wet, 1855); assert(!retained.needsWater);
  // A wet interruption resets dry confirmation.
  resetGuidance(s); observeSoil(s, dry, dry, wet, 0);
  observeSoil(s, wet, dry, wet, 900);
  observeSoil(s, dry, dry, wet, 1800);
  observeSoil(s, dry, dry, wet, 2700); assert(!s.needsWater);
  observeSoil(s, dry, dry, wet, 3600); assert(s.needsWater);
  // Cold boot preserves only the confirmed alert; calibration resets it.
  resetGuidance(s, true); assert(s.needsWater && !s.dryCount && !s.wetCount);
  resetGuidance(s); assert(!s.needsWater);
  // Reversed custom calibrations still follow the relative scale.
  observeSoil(s, wet, wet, dry, 0);
  observeSoil(s, wet, wet, dry, 15);
  observeSoil(s, wet, wet, dry, 30); assert(s.needsWater);
  observeSoil(s, dry, wet, dry, 35);
  observeSoil(s, dry, wet, dry, 50);
  observeSoil(s, dry, wet, dry, 65); assert(!s.needsWater);
}
