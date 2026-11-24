#include "btn.h"
#include "memory_view.h"

#include <Arduino.h>
#include "Calibration.h"


extern unsigned long btn_time;
extern unsigned long e_mode_time;
extern int e_mode;
extern bool A_mode;


extern uint8_t ina226_slow_avg;
extern uint8_t ina226_norm_avg;
extern int Key;
extern int step_v;
extern int step_a;
extern int last;
extern int val;
extern float vv;
extern float aa;

void t_one(int ms, bool systemEvent = false);
bool completeBaseCalibration();
void beginCurrentCalibration();
void resetCalibrationEncoder();
extern ApplicationCalibrationController calibration;
void clearLeft4();
void clearRight4();
void displayLeftNumber(int value, bool reboot);
void displayRightNumber(int value, bool reboot);
void displayLeftFloat(float value, bool reboot);
void displayRightFloat(float value, bool reboot);
void resetInaReadTimer();
void setInaAveraging(uint8_t avg);

static const unsigned long BTN_DEBOUNCE_MS = 50;
static const unsigned long BTN_LONG_PRESS_MS = 800;
static const unsigned long BTN_MULTI_CLICK_GAP_MS = 400;
static const uint8_t BTN_MAX_CLICK_COUNT = 5;

static bool btnInitialized = false;
static unsigned long pressStartTime = 0;
static unsigned long releaseTime = 0;
static unsigned long lastEventTime = 0;
static unsigned long multiTimerStart = 0;
static bool prevBtnLevel = HIGH;
static bool longPressHandled = false;
static uint8_t shortClickCount = 0;
static uint8_t longClickCount = 0;
static bool waitingForMulti = false;

static volatile bool buttonEvent = false;
static volatile bool buttonLevel = HIGH;
static volatile unsigned long buttonEventTime = 0;

static void btnEnterCalibration()
{
  btn_time = millis();
  e_mode_time = millis();
  A_mode = false;

  t_one(100);
  clearLeft4();
  clearRight4();
  displayLeftNumber(step_v, true);
  displayRightFloat(vv, true);

  e_mode = 1;
  setInaAveraging(ina226_slow_avg);
  resetInaReadTimer();
  last = val;
  resetCalibrationEncoder();
}

static void btnToggleCalibrationValue()
{
  if (A_mode) {
    e_mode = 0;
    setInaAveraging(ina226_norm_avg);
    resetInaReadTimer();
    return;
  }
  A_mode = true;
  beginCurrentCalibration();
  e_mode_time = millis();
  btn_time = millis();

  clearLeft4();
  clearRight4();

  if (!A_mode) {
    displayLeftNumber(step_v, true);
    displayRightFloat(vv, true);
  } else {
    displayRightNumber(step_a, true);
    displayLeftFloat(aa, true);
  }

  setInaAveraging(ina226_slow_avg);
  resetInaReadTimer();
  t_one(100);
}

static void btnHandleSingleShortPress()
{
  if (calibration.firstVoltageCalibrationPending() || calibration.firstCurrentCalibrationPending()) {
    completeBaseCalibration();
    return;
  }
  switch (e_mode) {
    case 0:
      btnEnterCalibration();
      break;

    case 1:
      btnToggleCalibrationValue();
      break;

    case 2:
      toggleMemoryViewEdit();
      break;
  }
}

static void btnFeedbackSeries(uint8_t count, int beepMs)
{
  for (uint8_t i = 0; i < count; i++) {
    t_one(beepMs);
    delay(100);
  }
}

static void btnHandleShortPressSeries(uint8_t clickCount)
{
  if (clickCount != 1 && (calibration.firstVoltageCalibrationPending() || calibration.firstCurrentCalibrationPending())) return;
  switch (clickCount) {
    case 1:
      btnHandleSingleShortPress();
      break;

    case 2:
      if (e_mode == 0) {
        enterMemoryView();
      } else {
        btnFeedbackSeries(clickCount, 60);
      }
      break;

    case 3:
      btnFeedbackSeries(clickCount, 60);
      break;

    default:
      btnFeedbackSeries(clickCount, 60);
      break;
  }
}

static void btnHandleLongPressSeries(uint8_t clickCount)
{
  if (calibration.firstVoltageCalibrationPending() || calibration.firstCurrentCalibrationPending()) return;
  if (clickCount != 1) {
    btnFeedbackSeries(clickCount, 200);
    return;
  }

  if (e_mode == 2) {
    exitMemoryView();
  } else {
    enterMemoryView();
  }
}

void btnInit()
{
  prevBtnLevel = digitalRead(Key);
  buttonLevel = prevBtnLevel;
  buttonEvent = false;
  buttonEventTime = millis();
  pressStartTime = (prevBtnLevel == LOW) ? buttonEventTime : 0;
  releaseTime = 0;
  lastEventTime = 0;
  multiTimerStart = 0;
  longPressHandled = false;
  shortClickCount = 0;
  longClickCount = 0;
  waitingForMulti = false;
  btnInitialized = true;
}

void buttonISR()
{
  buttonLevel = digitalRead(Key);
  buttonEventTime = millis();
  buttonEvent = true;
}

void btn()
{
  if (!btnInitialized) {
    btnInit();
  }

  unsigned long now = millis();
  bool hasEvent = false;
  bool level = prevBtnLevel;
  unsigned long eventTime = now;

  noInterrupts();
  if (buttonEvent) {
    hasEvent = true;
    level = buttonLevel;
    eventTime = buttonEventTime;
    buttonEvent = false;
  }
  interrupts();

  if (hasEvent && level != prevBtnLevel && eventTime - lastEventTime >= BTN_DEBOUNCE_MS) {
    lastEventTime = eventTime;
    prevBtnLevel = level;

    if (level == LOW) {
      pressStartTime = eventTime;
      longPressHandled = false;
    } else {
      releaseTime = eventTime;
      unsigned long pressDuration = (releaseTime >= pressStartTime) ? (releaseTime - pressStartTime) : 0;

      if (pressDuration >= BTN_LONG_PRESS_MS) {
        if (longClickCount < BTN_MAX_CLICK_COUNT) {
          longClickCount++;
        }
      } else if (pressDuration >= BTN_DEBOUNCE_MS) {
        if (shortClickCount < BTN_MAX_CLICK_COUNT) {
          shortClickCount++;
        }
      }

      if (shortClickCount > 0 || longClickCount > 0) {
        waitingForMulti = true;
        multiTimerStart = eventTime;
      }
    }
  }

  if (prevBtnLevel == LOW && !longPressHandled && pressStartTime != 0 && now - pressStartTime >= BTN_LONG_PRESS_MS) {
    longPressHandled = true;
  }

  if (waitingForMulti && now - multiTimerStart >= BTN_MULTI_CLICK_GAP_MS) {
    if (longClickCount > 0) {
      btnHandleLongPressSeries(longClickCount);
    } else if (shortClickCount > 0) {
      btnHandleShortPressSeries(shortClickCount);
    }

    shortClickCount = 0;
    longClickCount = 0;
    waitingForMulti = false;
  }
}
