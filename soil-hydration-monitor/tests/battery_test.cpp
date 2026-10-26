#include "../src/battery.h"
#include <cassert>
#include <cmath>
int main() {
 assert(calibrateBatteryMv(3750) == 3822);
 assert(calibrateBatteryMv(0) == 0);
 assert(calibrateBatteryMv(2499) == 0);
 assert(calibrateBatteryMv(4501) == 0);
 assert(decodeBatteryMv(0) == 0);
 assert(decodeBatteryMv(encodeBatteryMv(3822)) == 3820);
 assert(decodeBatteryMv(191) == 3820); // existing records are not rescaled
 uint16_t previous = 0;
 for (uint32_t mv = 2500; mv <= 4500; ++mv) {
   uint16_t corrected = calibrateBatteryMv(mv);
   assert(corrected >= previous); previous = corrected;
   assert(std::fabs(corrected - mv * 1.0192) <= 0.501);
   uint8_t code = encodeBatteryMv(corrected);
   if (validBatteryMv(corrected)) {
     assert(std::abs(int(decodeBatteryMv(code))-int(corrected)) <= 10);
     assert(batteryPercent(corrected) >= 0 && batteryPercent(corrected) <= 100);
   } else assert(code == 0);
 }
}
