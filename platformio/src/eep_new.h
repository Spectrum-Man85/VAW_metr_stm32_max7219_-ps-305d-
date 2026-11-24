#include <EEPROM.h>
extern bool f_need_save;
void t_one(int ms, bool systemEvent);
#include "temp_fan.h"

extern int buz_pin;
extern int Key;
extern bool act_buzer;
extern bool sh;
extern int version;
extern int toneschema;
extern float critTempHs;
extern float hysteresis;
extern float r_sh;

struct Settings
{
  uint16_t magic;
  float reserved0;
  float kvv;
  float kaa;
  float voltageCal[11];
  float reserved14;
  float reserved15;
  float currentCal[10];
  float reserved26;
  float reserved27;
  float reserved28;
  float reserved29;
  float inaVConv;
  float inaAConv;
  float inaNormAvg;
  float inaSlowAvg;
  float minTempHs;
  float maxTempHs;
  float pwmMinDutyHs;
  float pwmMaxDutyHs;
  float minTempTr;
  float maxTempTr;
  float pwmMinDutyTr;
  float pwmMaxDutyTr;
  float critTempHs;
  float hysteresis;
  float reserved[6];
};

Settings cfg;

static bool validFloat(float value, float minValue, float maxValue)
{
  return isfinite(value) && value >= minValue && value <= maxValue;
}

static bool validInaSetting(float value)
{
  if (!isfinite(value)) {
    return false;
  }

  uint8_t byteValue = (uint8_t)value;
  return value == (float)byteValue && byteValue <= 7;
}

static bool settingsValid(const Settings& settings)
{
  if (settings.magic != version) {
    return false;
  }

  if (!validFloat(settings.kvv, 0.1f, 10.0f) || !validFloat(settings.kaa, 0.1f, 10.0f)) {
    return false;
  }

  for (uint8_t i = 0; i < 11; i++) {
    if (!validFloat(settings.voltageCal[i], 0.1f, 10.0f)) {
      return false;
    }
  }

  for (uint8_t i = 0; i < 10; i++) {
    if (!validFloat(settings.currentCal[i], 0.1f, 10.0f)) {
      return false;
    }
  }

  return validInaSetting(settings.inaVConv) &&
         validInaSetting(settings.inaAConv) &&
         validInaSetting(settings.inaNormAvg) &&
         validInaSetting(settings.inaSlowAvg) &&
         validFloat(settings.minTempHs, 20.0f, 100.0f) &&
         validFloat(settings.maxTempHs, 20.0f, 100.0f) &&
         validFloat(settings.pwmMinDutyHs, 0.0f, 255.0f) &&
         validFloat(settings.pwmMaxDutyHs, 0.0f, 255.0f);
}

static void fillSettingsFromGlobals()
{
  cfg.magic = version;
  cfg.reserved0 = 0.0f;
  cfg.kvv = kvv;
  cfg.kaa = kaa;

  cfg.voltageCal[0] = k1;
  cfg.voltageCal[1] = k2;
  cfg.voltageCal[2] = k3;
  cfg.voltageCal[3] = k4;
  cfg.voltageCal[4] = k5;
  cfg.voltageCal[5] = k6;
  cfg.voltageCal[6] = k7;
  cfg.voltageCal[7] = k8;
  cfg.voltageCal[8] = k9;
  cfg.voltageCal[9] = k10;
  cfg.voltageCal[10] = k11;
  cfg.reserved14 = k12;
  cfg.reserved15 = v12;

  cfg.currentCal[0] = ka1;
  cfg.currentCal[1] = ka2;
  cfg.currentCal[2] = ka3;
  cfg.currentCal[3] = ka4;
  cfg.currentCal[4] = ka5;
  cfg.currentCal[5] = ka6;
  cfg.currentCal[6] = ka7;
  cfg.currentCal[7] = ka8;
  cfg.currentCal[8] = ka9;
  cfg.currentCal[9] = ka10;
  cfg.reserved26 = 0.0f;
  cfg.reserved27 = 0.0f;
  cfg.reserved28 = 0.0f;
  cfg.reserved29 = 0.0f;

  cfg.inaVConv = ina226_v_conv;
  cfg.inaAConv = ina226_a_conv;
  cfg.inaNormAvg = ina226_norm_avg;
  cfg.inaSlowAvg = ina226_slow_avg;
  cfg.minTempHs = minTempHs;
  cfg.maxTempHs = maxTempHs;
  cfg.pwmMinDutyHs = PWM_MIN_DUTYHS;
  cfg.pwmMaxDutyHs = PWM_MAX_DUTYHS;
  cfg.minTempTr = minTempTr;
  cfg.maxTempTr = maxTempTr;
  cfg.pwmMinDutyTr = PWM_MIN_DUTYTR;
  cfg.pwmMaxDutyTr = PWM_MAX_DUTYTR;
  cfg.critTempHs = critTempHs;
  cfg.hysteresis = hysteresis;

  cfg.reserved[0] = (float)toneschema;
  cfg.reserved[1] = act_buzer ? 1.0f : 0.0f;
  cfg.reserved[2] = sh ? 1.0f : 0.0f;
  cfg.reserved[3] = r_sh;
  for (uint8_t i = 4; i < 6; i++) 
  {
    cfg.reserved[i] = 0.0f;
  }
}

static void applySettingsToGlobals()
{
  kvv = cfg.kvv;
  kaa = cfg.kaa;

  k1 = cfg.voltageCal[0];
  k2 = cfg.voltageCal[1];
  k3 = cfg.voltageCal[2];
  k4 = cfg.voltageCal[3];
  k5 = cfg.voltageCal[4];
  k6 = cfg.voltageCal[5];
  k7 = cfg.voltageCal[6];
  k8 = cfg.voltageCal[7];
  k9 = cfg.voltageCal[8];
  k10 = cfg.voltageCal[9];
  k11 = cfg.voltageCal[10];
  if (validFloat(cfg.reserved14, 0.1f, 10.0f)) k12 = cfg.reserved14;
  if (validFloat(cfg.reserved15, 0.1f, 50.0f)) v12 = cfg.reserved15;

  ka1 = cfg.currentCal[0];
  ka2 = cfg.currentCal[1];
  ka3 = cfg.currentCal[2];
  ka4 = cfg.currentCal[3];
  ka5 = cfg.currentCal[4];
  ka6 = cfg.currentCal[5];
  ka7 = cfg.currentCal[6];
  ka8 = cfg.currentCal[7];
  ka9 = cfg.currentCal[8];
  ka10 = cfg.currentCal[9];

  ina226_v_conv = (uint8_t)cfg.inaVConv;
  ina226_a_conv = (uint8_t)cfg.inaAConv;
  ina226_norm_avg = (uint8_t)cfg.inaNormAvg;
  ina226_slow_avg = (uint8_t)cfg.inaSlowAvg;
  minTempHs = cfg.minTempHs;
  maxTempHs = cfg.maxTempHs;
  PWM_MIN_DUTYHS = cfg.pwmMinDutyHs;
  PWM_MAX_DUTYHS = cfg.pwmMaxDutyHs;
  if (validFloat(cfg.minTempTr, 1.0f, 100.0f)) minTempTr = cfg.minTempTr;
  if (validFloat(cfg.maxTempTr, 1.0f, 100.0f)) maxTempTr = cfg.maxTempTr;
  if (validFloat(cfg.pwmMinDutyTr, 0.0f, 255.0f)) PWM_MIN_DUTYTR = cfg.pwmMinDutyTr;
  if (validFloat(cfg.pwmMaxDutyTr, 0.0f, 255.0f)) PWM_MAX_DUTYTR = cfg.pwmMaxDutyTr;
  if (validFloat(cfg.critTempHs, 0.0f, 100.0f)) critTempHs = cfg.critTempHs;
  if (validFloat(cfg.hysteresis, 0.0f, 10.0f)) hysteresis = cfg.hysteresis;
  if (validFloat(cfg.reserved[0], 1.0f, 3.0f) &&
      cfg.reserved[0] == (float)(int)cfg.reserved[0]) {
    toneschema = (int)cfg.reserved[0];
  }
  if (cfg.reserved[1] == 0.0f || cfg.reserved[1] == 1.0f) {
    act_buzer = cfg.reserved[1] == 1.0f;
  }
  if (cfg.reserved[2] == 0.0f || cfg.reserved[2] == 1.0f) {
    sh = cfg.reserved[2] == 1.0f;
  }
  if (isfinite(cfg.reserved[3])) {
    r_sh = cfg.reserved[3];
    if (r_sh < 0.001f) r_sh = 0.001f;
    if (r_sh > 0.5f) r_sh = 0.5f;
  }
  if (maxTempTr < minTempTr) maxTempTr = minTempTr;
  if (PWM_MAX_DUTYTR < PWM_MIN_DUTYTR) PWM_MAX_DUTYTR = PWM_MIN_DUTYTR;
  if (PWM_MAX_DUTYHS < PWM_MIN_DUTYHS) PWM_MAX_DUTYHS = PWM_MIN_DUTYHS;
}

static void saveBeep(uint8_t count)
{
  if (toneschema == 1) {
    return;
  }

  for (uint8_t i = 0; i < count; i++) {
    if (act_buzer) {
      digitalWrite(buz_pin, HIGH);
      delay(200);
      digitalWrite(buz_pin, LOW);
      delay(200);
    } else {
      tone(buz_pin, 1000, 200);
      delay(200);
      noTone(buz_pin);
      delay(200);
    }
  }
}


struct PeripheralRecord {
  uint32_t magic;
  float values[20];
  uint32_t checksum;
};
constexpr size_t kPeripheralOffset = 848;
static_assert(kCurrentCalibrationEepromOffset + sizeof(CurrentCalibrationEepromRecord) <= kPeripheralOffset,
              "Calibration and peripheral records overlap");
static_assert(kPeripheralOffset + sizeof(PeripheralRecord) <= kCalibrationEepromCapacity,
              "Peripheral settings exceed EEPROM page");
static uint32_t peripheralChecksum(const PeripheralRecord& record) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(PeripheralRecord, checksum); ++i) hash = (hash ^ bytes[i]) * 16777619UL;
  return hash;
}
static void peripheralValues(bool load, PeripheralRecord& record) {
  float values[] = {float(ina226_v_conv), float(ina226_a_conv), float(ina226_norm_avg), float(ina226_slow_avg),
      minTempHs, maxTempHs, PWM_MIN_DUTYHS, PWM_MAX_DUTYHS,
      minTempTr, maxTempTr, PWM_MIN_DUTYTR, PWM_MAX_DUTYTR,
      critTempHs, hysteresis, float(toneschema), float(act_buzer), float(sh), r_sh, 0, 0};
  if (!load) { memcpy(record.values, values, sizeof(values)); return; }
  cfg.inaVConv=record.values[0]; cfg.inaAConv=record.values[1];
  cfg.inaNormAvg=record.values[2]; cfg.inaSlowAvg=record.values[3];
  cfg.minTempHs=record.values[4]; cfg.maxTempHs=record.values[5];
  cfg.pwmMinDutyHs=record.values[6]; cfg.pwmMaxDutyHs=record.values[7];
  cfg.minTempTr=record.values[8]; cfg.maxTempTr=record.values[9];
  cfg.pwmMinDutyTr=record.values[10]; cfg.pwmMaxDutyTr=record.values[11];
  cfg.critTempHs=record.values[12]; cfg.hysteresis=record.values[13];
  for (int i=0;i<4;++i) cfg.reserved[i]=record.values[14+i];
  applySettingsToGlobals();
}
void writeEEPROM();
void readEEPROM() {
  // Read the old MAX record before the source controller fills its shared buffer.
  EEPROM.get(0, cfg);
  const bool legacyValid = settingsValid(cfg);
  if (legacyValid) applySettingsToGlobals();
  const auto load = calibration.beginStorage();
  PeripheralRecord peripheral{};
  fillSettingsFromGlobals();
  if (calibration.storageReady() && calibration.storageForTest().readBytes(kPeripheralOffset, &peripheral, sizeof(peripheral)) &&
      peripheral.magic == 0x4D415832UL && peripheral.checksum == peripheralChecksum(peripheral)) {
    peripheralValues(true, peripheral);
  }
  syncCalibrationBases();
  if (legacyValid) writeEEPROM();
  Serial.println(load.voltageLoaded ? "Voltage calibration loaded" : "Voltage calibration defaults; recalibrate MAX hardware");
  Serial.println(load.currentLoaded ? "Current calibration loaded" : "Current calibration defaults; recalibrate MAX hardware");
  // Do not reinterpret MAX's old nonuniform calibration table as the source's 100-point table.
  if (digitalRead(Key) == LOW) {
    delay(30);
    if (digitalRead(Key) == LOW) {
      calibration.beginFirstVoltageCalibration();
      syncCalibrationBases();
    }
  }
}
void writeEEPROM() {
  if (!calibration.storageReady()) return;
  calibration.mutableCurrentBaseFactor() = kaa;
  PeripheralRecord peripheral{};
  peripheral.magic = 0x4D415832UL;
  peripheralValues(false, peripheral);
  peripheral.checksum = peripheralChecksum(peripheral);
  auto& storage = calibration.storageForTest();
  uint8_t currentPending = 1;
  if (!storage.writeBytes(kPeripheralOffset, &peripheral, sizeof(peripheral)) ||
      !saveCurrentCalibrationEepromIfRequested(storage, calibration.currentFactors(),
          calibration.currentBaseFactor(), currentPending, calibration.currentBaseCalibrationCompleted())) return;
  f_need_save = false;
  t_one(100, true);
}
