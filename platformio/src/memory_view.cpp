#include "memory_view.h"

#include <Arduino.h>
#include <IWatchdog.h>

#include "INA226.h"
#include "temp_fan.h"

extern int toneschema;
extern float kaa;
extern float ka1;
extern float ka2;
extern float ka3;
extern float hysteresis;
extern float critTempHs;
extern float r_sh;
extern bool act_buzer;
extern bool sh;
extern uint8_t ina226_v_conv;
extern uint8_t ina226_a_conv;
extern uint8_t ina226_norm_avg;
extern uint8_t ina226_slow_avg;

extern INA226 ina;
extern bool f_need_save;
extern int e_mode;
extern unsigned long e_mode_time;
extern int last;
extern int val;

uint16_t ina226ConvTimeUs(uint8_t conv);
uint16_t ina226AvgCount(uint8_t avg);
void setInaAveraging(uint8_t avg);
void resetInaReadTimer();
void writeEEPROM();
void t_one(int ms, bool systemEvent);
void clearLeft4();
void clearRight4();
void displayLeftNumber(int value, bool reboot);
void displayRightUInt(int value, bool reboot);
void displayRightFloat(float value, bool reboot);

namespace {

constexpr int MIN_CELL = 1;
constexpr int CELL_COUNT = 19;
constexpr int MAX_CELL = CELL_COUNT;
constexpr int ENCODER_TICKS_PER_CELL = 4;
constexpr int EDIT_TICKS_PER_STEP = 4;
constexpr float FLOAT_EDIT_STEP = 0.0001f;
constexpr unsigned long BLINK_INTERVAL_MS = 250;

int currentCell = MIN_CELL;
int navigationTicks = 0;
int editTicks = 0;
bool editMode = false;
bool blinkState = false;
unsigned long blinkTime = 0;

float* const floatValues[CELL_COUNT] = {
  &kaa, nullptr, nullptr, nullptr, nullptr, nullptr,
  &ka1, &ka2, &ka3, nullptr, nullptr, nullptr, nullptr, nullptr,
  &hysteresis, nullptr, nullptr, nullptr, &r_sh
};

constexpr uint8_t INA226_CONV_OPTIONS[] = {
  INA226_CONV_140US, INA226_CONV_204US, INA226_CONV_332US,
  INA226_CONV_588US, INA226_CONV_1100US, INA226_CONV_2116US,
  INA226_CONV_4156US, INA226_CONV_8244US
};

constexpr uint8_t INA226_AVG_OPTIONS[] = {
  INA226_AVG_X1, INA226_AVG_X4, INA226_AVG_X16, INA226_AVG_X64,
  INA226_AVG_X128, INA226_AVG_X256, INA226_AVG_X512, INA226_AVG_X1024
};

template <size_t N>
uint8_t getNextOption(const uint8_t (&options)[N], uint8_t value, int direction)
{
  size_t index = 0;
  for (size_t i = 0; i < N; i++) {
    if (options[i] == value) {
      index = i;
      break;
    }
  }

  return options[(index + direction + N) % N];
}

void resetBlink()
{
  blinkState = true;
  blinkTime = millis();
}

bool consumePositiveStep()
{
  if (editTicks < EDIT_TICKS_PER_STEP) return false;
  editTicks -= EDIT_TICKS_PER_STEP;
  return true;
}

bool consumeNegativeStep()
{
  if (editTicks > -EDIT_TICKS_PER_STEP) return false;
  editTicks += EDIT_TICKS_PER_STEP;
  return true;
}

void displayValue()
{
  switch (currentCell) {
    case 2: displayRightUInt(ina226ConvTimeUs(ina226_v_conv), true); return;
    case 3: displayRightUInt(ina226ConvTimeUs(ina226_a_conv), true); return;
    case 4: displayRightUInt(ina226AvgCount(ina226_norm_avg), true); return;
    case 5: displayRightUInt(ina226AvgCount(ina226_slow_avg), true); return;
    case 6: displayRightUInt((int)minTempHs, true); return;
    case 7: displayRightUInt((int)maxTempHs, true); return;
    case 8: displayRightUInt((int)PWM_MIN_DUTYHS, true); return;
    case 9: displayRightUInt((int)PWM_MAX_DUTYHS, true); return;
    case 10: displayRightUInt((int)minTempTr, true); return;
    case 11: displayRightUInt((int)maxTempTr, true); return;
    case 12: displayRightUInt((int)PWM_MIN_DUTYTR, true); return;
    case 13: displayRightUInt((int)PWM_MAX_DUTYTR, true); return;
    case 14: displayRightUInt((int)critTempHs, true); return;
    case 16: displayRightUInt(toneschema, true); return;
    case 17: displayRightUInt(act_buzer ? 1 : 0, true); return;
    case 18: displayRightUInt(sh ? 1 : 0, true); return;
    default: displayRightFloat(*floatValues[currentCell - 1], true); return;
  }
}

void finishEdit()
{
  resetBlink();
  displayValue();
}

void editInaSetting()
{
  uint8_t* setting = nullptr;
  bool averaging = false;

  switch (currentCell) {
    case 2: setting = &ina226_v_conv; break;
    case 3: setting = &ina226_a_conv; break;
    case 4: setting = &ina226_norm_avg; averaging = true; break;
    case 5: setting = &ina226_slow_avg; averaging = true; break;
  }

  bool changed = false;
  while (consumePositiveStep()) {
    *setting = averaging ? getNextOption(INA226_AVG_OPTIONS, *setting, 1)
                         : getNextOption(INA226_CONV_OPTIONS, *setting, 1);
    f_need_save = true;
    changed = true;
  }
  while (consumeNegativeStep()) {
    *setting = averaging ? getNextOption(INA226_AVG_OPTIONS, *setting, -1)
                         : getNextOption(INA226_CONV_OPTIONS, *setting, -1);
    f_need_save = true;
    changed = true;
  }

  if (changed) {
    if (currentCell == 2) ina.setSampleTime(INA226_VBUS, ina226_v_conv);
    else if (currentCell == 3) ina.setSampleTime(INA226_VSHUNT, ina226_a_conv);
    else if (currentCell == 4) setInaAveraging(ina226_norm_avg);
    else setInaAveraging(ina226_slow_avg);
    resetInaReadTimer();
  }
}

void editFloat(float& value, float step, float minValue, float maxValue)
{
  while (consumePositiveStep()) {
    value += step;
    if (value > maxValue) value = maxValue;
    f_need_save = true;
  }
  while (consumeNegativeStep()) {
    value -= step;
    if (value < minValue) value = minValue;
    f_need_save = true;
  }
}

void editInteger(int& value, int minValue, int maxValue)
{
  while (consumePositiveStep()) {
    if (value < maxValue) {
      value++;
      f_need_save = true;
    }
  }
  while (consumeNegativeStep()) {
    if (value > minValue) {
      value--;
      f_need_save = true;
    }
  }
}

void editBoolean(bool& value)
{
  while (consumePositiveStep()) {
    if (!value) {
      value = true;
      f_need_save = true;
    }
  }
  while (consumeNegativeStep()) {
    if (value) {
      value = false;
      f_need_save = true;
    }
  }
}

void editCurrentValue()
{
  if (currentCell >= 2 && currentCell <= 5) {
    editInaSetting();
    finishEdit();
    return;
  }

  switch (currentCell) {
    case 6: editFloat(minTempHs, 1.0f, 30.0f, 100.0f); break;
    case 7: editFloat(maxTempHs, 1.0f, 30.0f, 100.0f); break;
    case 8: editFloat(PWM_MIN_DUTYHS, 1.0f, 0.0f, 255.0f); break;
    case 9: editFloat(PWM_MAX_DUTYHS, 1.0f, PWM_MIN_DUTYHS, 255.0f); break;
    case 10: editFloat(minTempTr, 1.0f, 1.0f, 100.0f); break;
    case 11: editFloat(maxTempTr, 1.0f, minTempTr, 100.0f); break;
    case 12: editFloat(PWM_MIN_DUTYTR, 1.0f, 0.0f, 255.0f); break;
    case 13: editFloat(PWM_MAX_DUTYTR, 1.0f, PWM_MIN_DUTYTR, 255.0f); break;
    case 14: editFloat(critTempHs, 1.0f, 0.0f, 100.0f); break;
    case 15: editFloat(hysteresis, 0.1f, 0.0f, 10.0f); break;
    case 16: editInteger(toneschema, 1, 3); break;
    case 17: editBoolean(act_buzer); break;
    case 18: editBoolean(sh); break;
    case 19: editFloat(r_sh, 0.001f, 0.001f, 0.5f); break;
    default: editFloat(*floatValues[currentCell - 1], FLOAT_EDIT_STEP, 0.0f, 1000000.0f); break;
  }

  finishEdit();
}

}

void enterMemoryView()
{
  e_mode = 2;
  e_mode_time = millis();
  currentCell = MIN_CELL;
  navigationTicks = 0;
  editTicks = 0;
  editMode = false;
  resetBlink();
  last = val;

  clearLeft4();
  clearRight4();
  displayLeftNumber(currentCell, true);
  displayValue();
  t_one(60, false);
  t_one(60, false);
}

void toggleMemoryViewEdit()
{
  if (e_mode != 2) return;

  editMode = !editMode;
  navigationTicks = 0;
  editTicks = 0;
  resetBlink();
  last = val;

  clearRight4();
  displayValue();
  t_one(60, false);
}

void exitMemoryView()
{
  if (e_mode != 2) return;

  e_mode_time = millis();
  e_mode = 0;
  editMode = false;
  navigationTicks = 0;
  editTicks = 0;

  if (f_need_save) {
    IWatchdog.reload();
    f_need_save = false;
    writeEEPROM();
    IWatchdog.reload();
  }

  setInaAveraging(ina226_norm_avg);
  resetInaReadTimer();
  t_one(100, false);
}

void updateMemoryViewDisplay()
{
  if (e_mode != 2) return;

  displayLeftNumber(currentCell, false);

  if (editMode && millis() - blinkTime >= BLINK_INTERVAL_MS) {
    blinkTime = millis();
    blinkState = !blinkState;
  }

  if (!editMode || blinkState) displayValue();
  else clearRight4();
}

void handleMemoryViewEncoder()
{
  if (e_mode != 2 || val == last) return;

  e_mode_time = millis();
  int encoderDelta = val - last;
  last = val;

  if (editMode) {
    editTicks += encoderDelta;
    editCurrentValue();
    return;
  }

  navigationTicks += encoderDelta;

  while (navigationTicks >= ENCODER_TICKS_PER_CELL) {
    if (currentCell >= MAX_CELL) {
      navigationTicks = 0;
      break;
    }
    currentCell++;
    navigationTicks -= ENCODER_TICKS_PER_CELL;
    editTicks = 0;
    clearRight4();
    displayValue();
  }

  while (navigationTicks <= -ENCODER_TICKS_PER_CELL) {
    if (currentCell <= MIN_CELL) {
      navigationTicks = 0;
      break;
    }
    currentCell--;
    navigationTicks += ENCODER_TICKS_PER_CELL;
    editTicks = 0;
    clearRight4();
    displayValue();
  }
}
