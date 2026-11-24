#pragma once

#include <stdint.h>

constexpr uint32_t inaConversionTimeUs(uint8_t conversionCode) {
  return conversionCode == 0b000 ? 140U
       : conversionCode == 0b001 ? 204U
       : conversionCode == 0b010 ? 332U
       : conversionCode == 0b011 ? 588U
       : conversionCode == 0b100 ? 1100U
       : conversionCode == 0b101 ? 2116U
       : conversionCode == 0b110 ? 4156U
       : conversionCode == 0b111 ? 8244U
                                 : 0U;
}

constexpr uint16_t inaAveragingCount(uint8_t averagingCode) {
  return averagingCode == 0b000 ? 1U
       : averagingCode == 0b001 ? 4U
       : averagingCode == 0b010 ? 16U
       : averagingCode == 0b011 ? 64U
       : averagingCode == 0b100 ? 128U
       : averagingCode == 0b101 ? 256U
       : averagingCode == 0b110 ? 512U
       : averagingCode == 0b111 ? 1024U
                                : 0U;
}

constexpr uint32_t inaMeasurementCycleUs(uint8_t voltageConversionCode,
                                         uint8_t currentConversionCode,
                                         uint8_t averagingCode) {
  return (inaConversionTimeUs(voltageConversionCode) +
          inaConversionTimeUs(currentConversionCode)) *
         inaAveragingCount(averagingCode);
}

constexpr uint32_t microsecondsToMillisecondsCeil(uint32_t microseconds) {
  return (microseconds + 999U) / 1000U;
}

#if !defined(INA226_TIMING_ONLY)
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#define INA226_VBUS true
#define INA226_VSHUNT false

#define INA226_CONV_140US 0b000
#define INA226_CONV_204US 0b001
#define INA226_CONV_332US 0b010
#define INA226_CONV_588US 0b011
#define INA226_CONV_1100US 0b100
#define INA226_CONV_2116US 0b101
#define INA226_CONV_4156US 0b110
#define INA226_CONV_8244US 0b111

#define INA226_AVG_X1 0b000
#define INA226_AVG_X4 0b001
#define INA226_AVG_X16 0b010
#define INA226_AVG_X64 0b011
#define INA226_AVG_X128 0b100
#define INA226_AVG_X256 0b101
#define INA226_AVG_X512 0b110
#define INA226_AVG_X1024 0b111

#define INA226_CFG_REG_ADDR 0x00
#define INA226_SHUNT_REG_ADDR 0x01
#define INA226_VBUS_REG_ADDR 0x02
#define INA226_CUR_REG_ADDR 0x04
#define INA226_CAL_REG_ADDR 0x05

class INA226 {
 public:
  INA226(const float r_shunt = 0.1f, const float i_max = 0.8f,
         const uint8_t address = 0x40)
      : _r_shunt(r_shunt), _i_max(i_max), _iic_address(address) {}

  bool begin(int __attribute__((unused)) sda = 0,
             int __attribute__((unused)) scl = 0) {
#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_STM32)
    if (sda || scl) {
      Wire.begin(sda, scl);
    } else {
      Wire.begin();
    }
#else
    Wire.begin();
#endif
    if (!testConnection()) return false;
    calibrate();
    return true;
  }

  void setCalibration(uint16_t cal) {
    writeRegister(INA226_CAL_REG_ADDR, cal);
    _cal_value = cal;
  }

  void setAveraging(uint8_t avg) {
    uint16_t cfg_register =
        readRegister(INA226_CFG_REG_ADDR) & ~(0b111 << 9);
    writeRegister(INA226_CFG_REG_ADDR, cfg_register | avg << 9);
  }

  void setSampleTime(bool ch, uint8_t mode) {
    uint16_t cfg_register = readRegister(INA226_CFG_REG_ADDR);
    cfg_register &= ~((0b111) << (ch ? 6 : 3));
    cfg_register |= mode << (ch ? 6 : 3);
    writeRegister(INA226_CFG_REG_ADDR, cfg_register);
  }

  float getShuntVoltage() {
    setCalibration(_cal_value);
    const int16_t value = readRegister(INA226_SHUNT_REG_ADDR);
    return value * 0.0000025f;
  }

  float getVoltage() {
    const uint16_t value = readRegister(INA226_VBUS_REG_ADDR);
    return value * 0.00125f;
  }

  float getCurrent() {
    setCalibration(_cal_value);
    const int16_t value = readRegister(INA226_CUR_REG_ADDR);
    return value * _current_lsb;
  }

 private:
  const float _r_shunt = 0.0f;
  const float _i_max = 0.0f;
  const uint8_t _iic_address = 0x00;

  float _current_lsb = 0.0f;
  uint16_t _cal_value = 0;

  void writeRegister(uint8_t address, uint16_t data) {
    Wire.beginTransmission(_iic_address);
    Wire.write(address);
    Wire.write(highByte(data));
    Wire.write(lowByte(data));
    Wire.endTransmission();
  }

  uint16_t readRegister(uint8_t address) {
    Wire.beginTransmission(_iic_address);
    Wire.write(address);
    Wire.endTransmission();
    Wire.requestFrom(_iic_address, static_cast<uint8_t>(2));
    return Wire.read() << 8 | Wire.read();
  }

  bool testConnection() {
    Wire.beginTransmission(_iic_address);
    return !Wire.endTransmission();
  }

  void calibrate() {
    writeRegister(INA226_CFG_REG_ADDR, 0x8000);

    _current_lsb = _i_max / 32768.0f;
    _cal_value = trunc(0.00512f / (_current_lsb * _r_shunt));
    setCalibration(_cal_value);
  }
};

#endif  // !INA226_TIMING_ONLY
