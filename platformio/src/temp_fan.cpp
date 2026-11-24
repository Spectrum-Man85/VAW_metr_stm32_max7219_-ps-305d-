#include "temp_fan.h"

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

static const int tempPin1 = PA0;
static const int tempPin2 = PA1;

static const int fav1_pin = PA10;
static const int fav2_pin = PA9;

static uint8_t ss1 = 0;
static uint8_t ss2 = 0;

enum TempState { REQUEST_TEMP, WAIT_CONVERSION, READ_TEMP, IDLE };
static TempState tempState = IDLE;

static const uint32_t PWM_FREQ = 25000;
float PWM_MIN_DUTYHS = 50.0f;
float PWM_MIN_DUTYTR = 50.0f;
float PWM_MAX_DUTYHS = 255.0f;
float PWM_MAX_DUTYTR = 255.0f;

float minTempHs = 29.0f;
float maxTempHs = 39.0f;
float minTempTr = 29.0f;
float maxTempTr = 39.0f;
static const float criticalTempTr = 32.0f;

static int t_res = 10;
static int t_c_time = 188;

static float temperatureHs = 0;
static float temperatureTr = 0;
static float tt = 0.0f;
static float k = 0.5f;
static int currentSensor = 0;
static unsigned long fan_time = 0;

static HardwareTimer *pwmTimer = nullptr;

static OneWire oneWire1(tempPin1);
static DallasTemperature sensors1(&oneWire1);

static OneWire oneWire2(tempPin2);
static DallasTemperature sensors2(&oneWire2);

static float asyncReadTemperatures(uint8_t sensor)
{
  static uint32_t lastTempRequest = 0;
  float temp = 0.0f;

  switch (tempState) {
    case IDLE:
      if (millis() - lastTempRequest >= 2 * t_c_time) {
        tempState = REQUEST_TEMP;
      }
      break;

    case REQUEST_TEMP:
      if (sensor == 0) {
        sensors1.requestTemperatures();
      } else {
        sensors2.requestTemperatures();
      }
      tempState = WAIT_CONVERSION;
      lastTempRequest = millis();
      break;

    case WAIT_CONVERSION:
      if (millis() - lastTempRequest >= t_c_time) {
        tempState = READ_TEMP;
      }
      break;

    case READ_TEMP:
      if (sensor == 0) {
        temp = sensors1.getTempCByIndex(0);
      } else {
        temp = sensors2.getTempCByIndex(0);
      }
      tempState = REQUEST_TEMP;
      break;
  }

  return temp;
}

static int get_fan1(float temperature)
{
  int speed = 0;

  if (temperature < minTempHs) {
    speed = 0;
  } else if (temperature >= minTempHs && temperature < maxTempHs) {
    speed = PWM_MIN_DUTYHS + (temperature - minTempHs) * (PWM_MAX_DUTYHS - PWM_MIN_DUTYHS) / (maxTempHs - minTempHs);
  } else if (temperature >= maxTempHs) {
    speed = PWM_MAX_DUTYHS;
  }

  return speed;
}

static int get_fan2(float temperature)
{
  int speed = 0;

  if (temperature < minTempTr) {
    speed = 0;
  } else if (temperature >= minTempTr && temperature < maxTempTr) {
    speed = PWM_MIN_DUTYTR + (temperature - minTempTr) * (PWM_MAX_DUTYTR - PWM_MIN_DUTYTR) / (maxTempTr - minTempTr);
  } else if (temperature >= maxTempTr) {
    speed = PWM_MAX_DUTYTR;
  }

  return speed;
}

static void updateFanPwm()
{
  if (millis() - fan_time > 100) {
    pwmTimer->setCaptureCompare(3, ss1, RESOLUTION_8B_COMPARE_FORMAT);
    pwmTimer->setCaptureCompare(2, ss2, RESOLUTION_8B_COMPARE_FORMAT);
    fan_time = millis();
  }
}

void tempFanInit()
{
  (void)criticalTempTr;

  pinMode(fav1_pin, OUTPUT);
  pinMode(fav2_pin, OUTPUT);

  sensors1.begin();
  sensors2.begin();
  sensors1.setResolution(t_res);
  sensors2.setResolution(t_res);
  sensors1.setWaitForConversion(false);
  sensors2.setWaitForConversion(false);

  pwmTimer = new HardwareTimer(TIM1);
  pwmTimer->setOverflow(PWM_FREQ, HERTZ_FORMAT);
  pwmTimer->setMode(3, TIMER_OUTPUT_COMPARE_PWM1, fav1_pin);
  pwmTimer->setMode(2, TIMER_OUTPUT_COMPARE_PWM1, fav2_pin);
  pwmTimer->resume();
}

void tempFanTick()
{
  tt = asyncReadTemperatures(currentSensor);

  if (tt > 0 && currentSensor == 0) {
    temperatureHs += (tt - temperatureHs) * k;
    ss1 += (get_fan1(temperatureHs) - ss1) * k;
    tt = 0;
    currentSensor = 1;
  }

  if (tt > 0 && currentSensor == 1) {
    temperatureTr += (tt - temperatureTr) * k;
    ss2 += (get_fan2(temperatureTr) - ss2) * k;
    tt = 0;
    currentSensor = 0;
  }

  updateFanPwm();
}
