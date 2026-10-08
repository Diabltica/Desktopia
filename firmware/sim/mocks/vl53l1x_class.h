#pragma once
// Host-side stand-in for the STM32duino VL53L1X driver: same method names as the ones the firmware calls.
#include <stdint.h>
#include "Wire.h"
struct SimTof;                               // implemented by the simulator
extern SimTof* g_simTof;
int8_t simTofCall(const char* what);         // returns 0 when the sensor answers; flags a violation when it does not ACK
void simTofReady(uint8_t* r);
void simTofStatus(uint8_t* s);
void simTofDistance(uint16_t* d);
void simTofClear();
class VL53L1X {
 public:
  VL53L1X(TwoWire*, int) {}
  int begin() { return 0; }
  int8_t VL53L1X_BootState(uint8_t* s) { *s = 1; return simTofCall("BootState"); }
  int8_t VL53L1X_SensorInit() { return simTofCall("SensorInit"); }
  int8_t VL53L1X_GetSensorId(uint16_t* id) { *id = 0xEACC; return simTofCall("GetSensorId"); }
  int8_t VL53L1X_SetDistanceMode(uint16_t) { return simTofCall("SetDistanceMode"); }
  int8_t VL53L1X_SetTimingBudgetInMs(uint16_t) { return simTofCall("SetTimingBudget"); }
  int8_t VL53L1X_SetInterMeasurementInMs(uint16_t) { return simTofCall("SetInterMeasurement"); }
  int8_t VL53L1X_StartRanging() { return simTofCall("StartRanging"); }
  int8_t VL53L1X_CheckForDataReady(uint8_t* r) { int8_t e = simTofCall("CheckForDataReady"); simTofReady(r); return e; }
  int8_t VL53L1X_GetRangeStatus(uint8_t* s) { int8_t e = simTofCall("GetRangeStatus"); simTofStatus(s); return e; }
  int8_t VL53L1X_GetDistance(uint16_t* d) { int8_t e = simTofCall("GetDistance"); simTofDistance(d); return e; }
  int8_t VL53L1X_ClearInterrupt() { simTofClear(); return simTofCall("ClearInterrupt"); }
};
