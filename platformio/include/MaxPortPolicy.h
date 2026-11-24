#pragma once
#include <stdint.h>
inline uint32_t maxVoltageStepMultiplier(float voltage, uint32_t multiplier) {
  return multiplier * (voltage >= 9.9995f ? 10U : 1U);
}
inline uint32_t maxCurrentStepMultiplier(bool baseStage, uint32_t multiplier) {
  return multiplier * (baseStage ? 10U : 1U);
}
inline float maxCompensatedVoltage(float calibratedVoltage, float calibratedCurrent,
                                   float currentBaseFactor, float shuntResistance, bool enabled) {
  const float result = enabled && currentBaseFactor > 0
      ? calibratedVoltage - calibratedCurrent * shuntResistance / currentBaseFactor
      : calibratedVoltage;
  return result > 0 ? result : 0;
}
struct MaxFourDigitNumber { int digits; int decimalPosition; bool valid; };
inline MaxFourDigitNumber maxFourDigitNumber(float value) {
  if (!(value >= 0 && value <= 9999.0f)) return {0, 0, false};
  int decimalPosition = 3;
  float scale = 1000;
  int digits = static_cast<int>(value * scale + 0.5f);
  while (digits > 9999 && decimalPosition > 0) {
    --decimalPosition;
    scale *= 0.1f;
    digits = static_cast<int>(value * scale + 0.5f);
  }
  return {digits, decimalPosition, true};
}
