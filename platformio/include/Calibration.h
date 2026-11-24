#pragma once

#include <cfloat>
#include <cstddef>
#include <cstdint>

// Coefficients belong to range centers. Clamp outside the outer centers.
// Search using the uncorrected input; do not feed corrected values back.
template <size_t N>
constexpr float interpolatedCalibrationFactor(
    float value, const float (&factors)[N], unsigned stepMilli) {
  static_assert(N > 0, "Calibration table must not be empty");
  const float first = static_cast<float>(stepMilli) / 2000.0f;
  const float last = static_cast<float>((2 * N - 1) * stepMilli) / 2000.0f;
  if (!(value > first)) return factors[0];
  if (value >= last) return factors[N - 1];
  size_t low = 0, high = N - 1;
  while (low + 1 < high) {
    const size_t middle = low + (high - low) / 2;
    const float center = static_cast<float>((2 * middle + 1) * stepMilli) / 2000.0f;
    if (value < center) high = middle;
    else low = middle;
  }
  const float left = static_cast<float>((2 * low + 1) * stepMilli) / 2000.0f;
  const float right = static_cast<float>((2 * high + 1) * stepMilli) / 2000.0f;
  const float weight = (value - left) / (right - left);
  return factors[low] + (factors[high] - factors[low]) * weight;
}

constexpr size_t VOLTAGE_CALIBRATION_COUNT = 100;
constexpr unsigned VOLTAGE_CALIBRATION_STEP_MV = 360;
constexpr float VOLTAGE_CALIBRATION_FACTOR_DEFAULT = 1.00f;

inline float& voltageCalibrationCoefficientForEdit(
    bool firstStagePending, size_t index, float& baseFactor,
    float (&rangeFactors)[VOLTAGE_CALIBRATION_COUNT]) {
  return firstStagePending ? baseFactor : rangeFactors[index];
}

inline float rescaleVoltageAfterBaseFactorChange(
    float voltage, float previousBaseFactor, float updatedBaseFactor) {
  return voltage * (updatedBaseFactor / previousBaseFactor);
}
constexpr int voltageDisplayDigits(float voltage) {
  if (!(voltage >= 0.0f)) return 0;
  return static_cast<int>(voltage * 1000.0f + 0.5f);
}

// Move by the requested number of displayed 1 mV intervals.
inline bool voltageCalibrationDisplayStepTarget(
    float voltage, bool increase, int& targetDigits, float& targetVoltage,
    uint32_t steps = 1) {
  if (!(voltage >= 0.0f && voltage < 2147483.0f) || steps == 0) return false;
  const int displayedDigits = voltageDisplayDigits(voltage);
  if (!increase && displayedDigits == 0) return false;
  uint32_t target = static_cast<uint32_t>(displayedDigits);
  if (increase) {
    if (target >= 2147483000U || steps >= 2147483000U - target) return false;
    target += steps;
  } else {
    target = steps >= target ? 0U : target - steps;
  }
  targetDigits = static_cast<int>(target);
  targetVoltage = target == 0 ? 0.00049f : static_cast<float>(targetDigits) * 0.001f;
  return targetVoltage > 0.0f && targetVoltage < 2147483.0f &&
      voltageDisplayDigits(targetVoltage) == targetDigits;
}

// Диапазоны выбираются по напряжению ДО коррекции: [нижняя, верхняя).
// Значения по умолчанию 1.0f: используются при пустой или повреждённой EEPROM.
// Новый коэффициент = текущий * эталонное напряжение / показанное напряжение.
inline __attribute__((noinline)) void fillVoltageCalibrationDefaults(
    float (&factors)[VOLTAGE_CALIBRATION_COUNT]) {
  for (size_t i = 0; i < VOLTAGE_CALIBRATION_COUNT; ++i) {
    factors[i] = VOLTAGE_CALIBRATION_FACTOR_DEFAULT;
  }
}
constexpr size_t voltageCalibrationIndex(float voltage) {
  // Сравниваем с границами вместо деления: избегаем округления индекса
  // вниз на точной границе. Крайние участки включают выход за диапазон.
  if (!(voltage > 0.0f)) return 0;
  size_t low = 0;
  size_t high = VOLTAGE_CALIBRATION_COUNT;
  while (low + 1 < high) {
    const size_t middle = low + (high - low) / 2;
    const float boundary =
        static_cast<float>(middle * VOLTAGE_CALIBRATION_STEP_MV) / 1000.0f;
    if (voltage < boundary) high = middle;
    else low = middle;
  }
  return low;
}

constexpr float calibrateVoltage(
    float voltage,
    const float (&factors)[VOLTAGE_CALIBRATION_COUNT]) {
  return voltage * interpolatedCalibrationFactor(
      voltage, factors, VOLTAGE_CALIBRATION_STEP_MV);
}
// Return the contribution of one table entry to the interpolated factor.
inline float voltageCalibrationCoefficientWeight(float voltage,
                                                 size_t coefficientIndex) {
  if (coefficientIndex >= VOLTAGE_CALIBRATION_COUNT) return 0.0f;
  const float first =
      static_cast<float>(VOLTAGE_CALIBRATION_STEP_MV) / 2000.0f;
  const float last = static_cast<float>(
      (2 * VOLTAGE_CALIBRATION_COUNT - 1) * VOLTAGE_CALIBRATION_STEP_MV) /
      2000.0f;
  if (!(voltage > first)) return coefficientIndex == 0 ? 1.0f : 0.0f;
  if (voltage >= last) {
    return coefficientIndex == VOLTAGE_CALIBRATION_COUNT - 1 ? 1.0f : 0.0f;
  }

  size_t low = 0;
  size_t high = VOLTAGE_CALIBRATION_COUNT - 1;
  while (low + 1 < high) {
    const size_t middle = low + (high - low) / 2;
    const float center = static_cast<float>(
        (2 * middle + 1) * VOLTAGE_CALIBRATION_STEP_MV) / 2000.0f;
    if (voltage < center) high = middle;
    else low = middle;
  }
  const float left = static_cast<float>(
      (2 * low + 1) * VOLTAGE_CALIBRATION_STEP_MV) / 2000.0f;
  const float right = static_cast<float>(
      (2 * high + 1) * VOLTAGE_CALIBRATION_STEP_MV) / 2000.0f;
  const float highWeight = (voltage - left) / (right - left);
  if (coefficientIndex == low) return 1.0f - highWeight;
  if (coefficientIndex == high) return highWeight;
  return 0.0f;
}

// Solve the base factor against the actual range-calibration curve.
inline bool solveVoltageBaseFactorForTarget(
    float measuredVoltage, float previousBaseFactor, float targetVoltage,
    const float (&factors)[VOLTAGE_CALIBRATION_COUNT],
    float& updatedBaseFactor) {
  if (!(measuredVoltage > 0.0f) || !(previousBaseFactor > 0.0f) ||
      !(targetVoltage > 0.0f)) return false;

  const float current = calibrateVoltage(measuredVoltage, factors);
  if (!(current >= 0.0f)) return false;
  if (current == targetVoltage) {
    updatedBaseFactor = previousBaseFactor;
    return true;
  }

  float low = previousBaseFactor;
  float high = previousBaseFactor;
  bool bracketed = false;
  if (targetVoltage > current) {
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
      if (high > FLT_MAX / 2.0f) return false;
      high *= 2.0f;
      const float value = calibrateVoltage(
          rescaleVoltageAfterBaseFactorChange(
              measuredVoltage, previousBaseFactor, high), factors);
      if (value >= targetVoltage) {
        bracketed = true;
        break;
      }
      low = high;
    }
  } else {
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
      low *= 0.5f;
      const float value = calibrateVoltage(
          rescaleVoltageAfterBaseFactorChange(
              measuredVoltage, previousBaseFactor, low), factors);
      if (value <= targetVoltage) {
        bracketed = true;
        break;
      }
      high = low;
    }
  }
  if (!bracketed) return false;

  for (unsigned attempt = 0; attempt < 24; ++attempt) {
    const float middle = low + (high - low) * 0.5f;
    const float value = calibrateVoltage(
        rescaleVoltageAfterBaseFactorChange(
            measuredVoltage, previousBaseFactor, middle), factors);
    if (value < targetVoltage) low = middle;
    else high = middle;
  }
  updatedBaseFactor = low + (high - low) * 0.5f;
  return updatedBaseFactor > 0.0f && updatedBaseFactor <= FLT_MAX;
}

constexpr size_t CURRENT_CALIBRATION_COUNT = 100;
constexpr float CURRENT_CALIBRATION_FACTOR_DEFAULT = 1.0f;
constexpr float CURRENT_FINE_DISPLAY_LIMIT_A = 9.99995f;

constexpr bool usesFineCurrentCalibrationDisplay(float currentA,
                                              bool baseCalibrationPending) {
  return baseCalibrationPending && currentA >= 0.0f &&
         currentA < CURRENT_FINE_DISPLAY_LIMIT_A;
}

constexpr int currentDisplayDigits(float currentA, bool baseCalibrationPending) {
  if (!(currentA >= 0.0f)) return 0;
  if (usesFineCurrentCalibrationDisplay(currentA, baseCalibrationPending)) {
    return static_cast<int>(currentA * 10000.0f + 0.5f);
  }
  const float milliampDigits = currentA * 1000.0f;
  return static_cast<int>(milliampDigits + 0.5f);
}

// Move along displayed digit intervals, including the 9.9999 / 10.000 boundary.
inline bool currentCalibrationDisplayStepTarget(
    float currentA, bool baseCalibrationPending, bool increase,
    int& targetDigits, float& targetCurrentA, uint32_t steps = 1) {
  if (!(currentA >= 0.0f && currentA < 2147483.0f) || steps == 0) return false;
  const int displayedDigits = currentDisplayDigits(currentA, baseCalibrationPending);
  if (!increase && displayedDigits == 0) return false;
  const bool fine = usesFineCurrentCalibrationDisplay(currentA, baseCalibrationPending);
  // A continuous index counts 100000 fine intervals below 10 A, then mA intervals.
  uint32_t target = static_cast<uint32_t>(displayedDigits);
  if (baseCalibrationPending && !fine) target += 90000U;
  if (increase) {
    if (steps > 0xFFFFFFFFU - target) return false;
    target += steps;
  } else {
    target = steps >= target ? 0U : target - steps;
  }
  if (baseCalibrationPending && target < 100000) {
    targetDigits = static_cast<int>(target);
    targetCurrentA = target == 0 ? 0.000025f : static_cast<float>(target) * 0.0001f;
  } else {
    if (baseCalibrationPending) target -= 90000U;
    if (target >= 2147483000U) return false;
    targetDigits = static_cast<int>(target);
    targetCurrentA = targetDigits == 0
        ? 0.00049f
        : static_cast<float>(targetDigits) * 0.001f;
  }
  return targetCurrentA > 0.0f && targetCurrentA < 2147483.0f &&
      currentDisplayDigits(targetCurrentA, baseCalibrationPending) == targetDigits;
}
inline float& currentCalibrationCoefficientForEdit(
    bool firstStagePending, size_t index, float& baseFactor,
    float (&rangeFactors)[CURRENT_CALIBRATION_COUNT]) {
  return firstStagePending ? baseFactor : rangeFactors[index];
}

constexpr unsigned CURRENT_CALIBRATION_STEP_MA = 100;

inline __attribute__((noinline)) void fillCurrentCalibrationDefaults(
    float (&factors)[CURRENT_CALIBRATION_COUNT]) {
  for (size_t i = 0; i < CURRENT_CALIBRATION_COUNT; ++i) {
    factors[i] = CURRENT_CALIBRATION_FACTOR_DEFAULT;
  }
}

constexpr size_t currentCalibrationIndex(float current) {
  // Сравниваем с границами вместо деления: избегаем округления индекса
  // вниз на точной границе. Крайние участки включают выход за диапазон.
  if (!(current > 0.0f)) return 0;
  size_t low = 0;
  size_t high = CURRENT_CALIBRATION_COUNT;
  while (low + 1 < high) {
    const size_t middle = low + (high - low) / 2;
    const float boundary =
        static_cast<float>(middle * CURRENT_CALIBRATION_STEP_MA) / 1000.0f;
    if (current < boundary) high = middle;
    else low = middle;
  }
  return low;
}

constexpr float calibrateCurrent(
    float current,
    const float (&factors)[CURRENT_CALIBRATION_COUNT]) {
  return current * interpolatedCalibrationFactor(
      current, factors, CURRENT_CALIBRATION_STEP_MA);
}
// Return the contribution of one table entry to the interpolated factor.
inline float currentCalibrationCoefficientWeight(float current,
                                                 size_t coefficientIndex) {
  if (coefficientIndex >= CURRENT_CALIBRATION_COUNT) return 0.0f;
  const float first =
      static_cast<float>(CURRENT_CALIBRATION_STEP_MA) / 2000.0f;
  const float last = static_cast<float>(
      (2 * CURRENT_CALIBRATION_COUNT - 1) * CURRENT_CALIBRATION_STEP_MA) /
      2000.0f;
  if (!(current > first)) return coefficientIndex == 0 ? 1.0f : 0.0f;
  if (current >= last) {
    return coefficientIndex == CURRENT_CALIBRATION_COUNT - 1 ? 1.0f : 0.0f;
  }

  size_t low = 0;
  size_t high = CURRENT_CALIBRATION_COUNT - 1;
  while (low + 1 < high) {
    const size_t middle = low + (high - low) / 2;
    const float center = static_cast<float>(
        (2 * middle + 1) * CURRENT_CALIBRATION_STEP_MA) / 2000.0f;
    if (current < center) high = middle;
    else low = middle;
  }
  const float left = static_cast<float>(
      (2 * low + 1) * CURRENT_CALIBRATION_STEP_MA) / 2000.0f;
  const float right = static_cast<float>(
      (2 * high + 1) * CURRENT_CALIBRATION_STEP_MA) / 2000.0f;
  const float highWeight = (current - left) / (right - left);
  if (coefficientIndex == low) return 1.0f - highWeight;
  if (coefficientIndex == high) return highWeight;
  return 0.0f;
}

// Solve the base factor against the actual range-calibration curve.
inline bool solveCurrentBaseFactorForTarget(
    float measuredCurrent, float previousBaseFactor, float targetCurrent,
    const float (&factors)[CURRENT_CALIBRATION_COUNT],
    float& updatedBaseFactor) {
  if (!(measuredCurrent > 0.0f) || !(previousBaseFactor > 0.0f) ||
      !(targetCurrent > 0.0f)) return false;

  const float current =
      calibrateCurrent(measuredCurrent * previousBaseFactor, factors);
  if (!(current >= 0.0f)) return false;
  if (current == targetCurrent) {
    updatedBaseFactor = previousBaseFactor;
    return true;
  }

  float low = previousBaseFactor;
  float high = previousBaseFactor;
  bool bracketed = false;
  if (targetCurrent > current) {
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
      if (high > FLT_MAX / 2.0f) return false;
      high *= 2.0f;
      const float value = calibrateCurrent(measuredCurrent * high, factors);
      if (value >= targetCurrent) {
        bracketed = true;
        break;
      }
      low = high;
    }
  } else {
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
      low *= 0.5f;
      const float value = calibrateCurrent(measuredCurrent * low, factors);
      if (value <= targetCurrent) {
        bracketed = true;
        break;
      }
      high = low;
    }
  }
  if (!bracketed) return false;

  for (unsigned attempt = 0; attempt < 24; ++attempt) {
    const float middle = low + (high - low) * 0.5f;
    const float value = calibrateCurrent(measuredCurrent * middle, factors);
    if (value < targetCurrent) low = middle;
    else high = middle;
  }
  updatedBaseFactor = low + (high - low) * 0.5f;
  return updatedBaseFactor > 0.0f && updatedBaseFactor <= FLT_MAX;
}



struct VoltageCalibrationAdjustment {
  bool changed = false;
  float correctedVoltage = 0.0f;
  float adjustedLoadVoltage = 0.0f;
  int displayDigits = 0;
};

struct CurrentCalibrationAdjustment {
  bool changed = false;
  float correctedCurrent = 0.0f;
  int displayDigits = 0;
};


#include <cstring>

#if defined(ARDUINO_ARCH_STM32)
#include <utility/stm32_eeprom.h>
#endif

template <size_t Count>
struct CalibrationEepromRecordV1 {
  uint32_t magic;
  uint32_t version;
  float baseFactor;
  float factors[Count];
  uint32_t checksum;
};

template <size_t Count>
struct CalibrationEepromRecord {
  uint32_t magic;
  uint32_t version;
  uint32_t flags;
  float baseFactor;
  float factors[Count];
  uint32_t checksum;
};

using VoltageCalibrationEepromRecordV1 =
    CalibrationEepromRecordV1<VOLTAGE_CALIBRATION_COUNT>;
using CurrentCalibrationEepromRecordV1 =
    CalibrationEepromRecordV1<CURRENT_CALIBRATION_COUNT>;
using VoltageCalibrationEepromRecord =
    CalibrationEepromRecord<VOLTAGE_CALIBRATION_COUNT>;
using CurrentCalibrationEepromRecord =
    CalibrationEepromRecord<CURRENT_CALIBRATION_COUNT>;

constexpr uint32_t kCalibrationLegacyVersion = 1U;
constexpr uint32_t kCalibrationEepromVersion = 2U;
constexpr uint32_t kCalibrationBaseCompletedFlag = 1U;
constexpr uint32_t kVoltageCalibrationMagic = 0x56434550UL;
constexpr uint32_t kCurrentCalibrationMagic = 0x49434550UL;
constexpr size_t kCalibrationEepromCapacity = 1024U;
constexpr size_t kVoltageCalibrationEepromOffset = 0U;
constexpr size_t kLegacyCurrentCalibrationEepromOffset =
    sizeof(VoltageCalibrationEepromRecordV1);
constexpr size_t kCurrentCalibrationEepromOffset =
    sizeof(VoltageCalibrationEepromRecord);

static_assert(sizeof(VoltageCalibrationEepromRecord) ==
                  sizeof(CurrentCalibrationEepromRecord),
              "Voltage and current EEPROM records must have the same size");
static_assert(kCurrentCalibrationEepromOffset +
                  sizeof(CurrentCalibrationEepromRecord) <=
                  kCalibrationEepromCapacity,
              "Calibration EEPROM records exceed the reserved flash page");
static_assert(kLegacyCurrentCalibrationEepromOffset +
                  sizeof(CurrentCalibrationEepromRecordV1) <=
                  kCalibrationEepromCapacity,
              "Legacy calibration EEPROM records exceed the reserved flash page");

template <size_t Count>
inline uint32_t calibrationEepromChecksumV1(
    const CalibrationEepromRecordV1<Count>& record) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0;
       i < offsetof(CalibrationEepromRecordV1<Count>, checksum); ++i) {
    hash = (hash ^ bytes[i]) * 16777619UL;
  }
  return hash;
}

template <size_t Count>
inline uint32_t calibrationEepromChecksum(
    const CalibrationEepromRecord<Count>& record) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0;
       i < offsetof(CalibrationEepromRecord<Count>, checksum); ++i) {
    hash = (hash ^ bytes[i]) * 16777619UL;
  }
  return hash;
}

inline bool validCalibrationEepromFactor(float factor) {
  return factor > 0.0f && factor <= FLT_MAX;
}

template <size_t Count>
inline bool validCalibrationEepromRecord(
    const CalibrationEepromRecord<Count>& record, uint32_t expectedMagic) {
  if (record.magic != expectedMagic ||
      record.version != kCalibrationEepromVersion ||
      (record.flags & ~kCalibrationBaseCompletedFlag) != 0U ||
      record.checksum != calibrationEepromChecksum(record) ||
      !validCalibrationEepromFactor(record.baseFactor)) {
    return false;
  }
  for (size_t i = 0; i < Count; ++i) {
    if (!validCalibrationEepromFactor(record.factors[i])) return false;
  }
  return true;
}

template <size_t Count>
inline bool validCalibrationEepromRecordV1(
    const CalibrationEepromRecordV1<Count>& record, uint32_t expectedMagic) {
  if (record.magic != expectedMagic ||
      record.version != kCalibrationLegacyVersion ||
      record.checksum != calibrationEepromChecksumV1(record) ||
      !validCalibrationEepromFactor(record.baseFactor)) {
    return false;
  }
  for (size_t i = 0; i < Count; ++i) {
    if (!validCalibrationEepromFactor(record.factors[i])) return false;
  }
  return true;
}

template <typename Storage, typename Record>
inline bool calibrationStorageRangeValid(
    Storage& storage, size_t offset) {
  return storage.capacity() >= offset &&
      sizeof(Record) <= storage.capacity() - offset;
}

template <typename Storage, typename Record>
inline bool readCalibrationEepromRecord(
    Storage& storage, size_t offset, uint32_t expectedMagic, Record& record) {
  if (!calibrationStorageRangeValid<Storage, Record>(storage, offset) ||
      !storage.readBytes(offset, &record, sizeof(record))) {
    return false;
  }
  return validCalibrationEepromRecord(record, expectedMagic);
}

template <typename Storage, typename Record>
inline bool readCalibrationEepromRecordV1(
    Storage& storage, size_t offset, uint32_t expectedMagic, Record& record) {
  if (!calibrationStorageRangeValid<Storage, Record>(storage, offset) ||
      !storage.readBytes(offset, &record, sizeof(record))) {
    return false;
  }
  return validCalibrationEepromRecordV1(record, expectedMagic);
}

template <typename Storage, typename Record, size_t Count>
inline bool stageCalibrationEepromRecordIfRequested(
    Storage& storage, size_t offset, uint32_t magic,
    const float (&factors)[Count], float baseFactor, uint32_t flags,
    uint8_t pending) {
  if (pending != 1 ||
      (flags & ~kCalibrationBaseCompletedFlag) != 0U ||
      !validCalibrationEepromFactor(baseFactor)) {
    return false;
  }
  for (size_t i = 0; i < Count; ++i) {
    if (!validCalibrationEepromFactor(factors[i])) return false;
  }
  if (!calibrationStorageRangeValid<Storage, Record>(storage, offset)) {
    return false;
  }

  Record record{};
  record.magic = magic;
  record.version = kCalibrationEepromVersion;
  record.flags = flags;
  record.baseFactor = baseFactor;
  std::memcpy(record.factors, factors, sizeof(record.factors));
  record.checksum = calibrationEepromChecksum(record);
  return storage.writeBytes(offset, &record, sizeof(record));
}

template <typename Storage, typename Record, size_t Count>
inline bool saveCalibrationEepromRecordIfRequested(
    Storage& storage, size_t offset, uint32_t magic,
    const float (&factors)[Count], float baseFactor, uint32_t flags,
    uint8_t& pending) {
  if (!stageCalibrationEepromRecordIfRequested<Storage, Record>(
          storage, offset, magic, factors, baseFactor, flags, pending)) {
    return false;
  }
  if (!storage.commit()) return false;
  pending = 0;
  return true;
}

enum class CalibrationEepromLoadResult : uint8_t {
  kInvalid,
  kLoadedV1,
  kLoadedV2,
};

enum class CalibrationEepromTransactionResult : uint8_t {
  kSaved,
  kVoltageRejected,
  kCurrentRejected,
  kCommitFailed,
};

template <typename Storage>
inline CalibrationEepromLoadResult loadVoltageCalibrationEeprom(
    Storage& storage, float (&factors)[VOLTAGE_CALIBRATION_COUNT],
    float& baseFactor, bool& baseCalibrationCompleted) {
  VoltageCalibrationEepromRecord record{};
  if (readCalibrationEepromRecord(
          storage, kVoltageCalibrationEepromOffset,
          kVoltageCalibrationMagic, record)) {
    std::memcpy(factors, record.factors, sizeof(factors));
    baseFactor = record.baseFactor;
    baseCalibrationCompleted =
        (record.flags & kCalibrationBaseCompletedFlag) != 0U;
    return CalibrationEepromLoadResult::kLoadedV2;
  }

  VoltageCalibrationEepromRecordV1 legacy{};
  if (readCalibrationEepromRecordV1(
          storage, kVoltageCalibrationEepromOffset,
          kVoltageCalibrationMagic, legacy)) {
    std::memcpy(factors, legacy.factors, sizeof(factors));
    baseFactor = legacy.baseFactor;
    baseCalibrationCompleted = true;
    return CalibrationEepromLoadResult::kLoadedV1;
  }
  return CalibrationEepromLoadResult::kInvalid;
}

template <typename Storage>
inline CalibrationEepromLoadResult loadCurrentCalibrationEeprom(
    Storage& storage, float (&factors)[CURRENT_CALIBRATION_COUNT],
    float& baseFactor, bool& baseCalibrationCompleted) {
  CurrentCalibrationEepromRecord record{};
  if (readCalibrationEepromRecord(
          storage, kCurrentCalibrationEepromOffset,
          kCurrentCalibrationMagic, record)) {
    std::memcpy(factors, record.factors, sizeof(factors));
    baseFactor = record.baseFactor;
    baseCalibrationCompleted =
        (record.flags & kCalibrationBaseCompletedFlag) != 0U;
    return CalibrationEepromLoadResult::kLoadedV2;
  }

  CurrentCalibrationEepromRecordV1 legacy{};
  if (readCalibrationEepromRecordV1(
          storage, kLegacyCurrentCalibrationEepromOffset,
          kCurrentCalibrationMagic, legacy)) {
    std::memcpy(factors, legacy.factors, sizeof(factors));
    baseFactor = legacy.baseFactor;
    baseCalibrationCompleted =
        baseFactor != CURRENT_CALIBRATION_FACTOR_DEFAULT;
    return CalibrationEepromLoadResult::kLoadedV1;
  }
  return CalibrationEepromLoadResult::kInvalid;
}

template <typename Storage>
inline bool loadVoltageCalibrationEeprom(
    Storage& storage, float (&factors)[VOLTAGE_CALIBRATION_COUNT],
    float& baseFactor) {
  bool completed = false;
  return loadVoltageCalibrationEeprom(
      storage, factors, baseFactor, completed) !=
      CalibrationEepromLoadResult::kInvalid;
}

template <typename Storage>
inline bool loadCurrentCalibrationEeprom(
    Storage& storage, float (&factors)[CURRENT_CALIBRATION_COUNT],
    float& baseFactor) {
  bool completed = false;
  return loadCurrentCalibrationEeprom(
      storage, factors, baseFactor, completed) !=
      CalibrationEepromLoadResult::kInvalid;
}

template <typename Storage>
inline bool saveVoltageCalibrationEepromIfRequested(
    Storage& storage, const float (&factors)[VOLTAGE_CALIBRATION_COUNT],
    float baseFactor, uint8_t& pending, bool baseCalibrationCompleted = true) {
  return saveCalibrationEepromRecordIfRequested<
      Storage, VoltageCalibrationEepromRecord>(
      storage, kVoltageCalibrationEepromOffset, kVoltageCalibrationMagic,
      factors, baseFactor,
      baseCalibrationCompleted ? kCalibrationBaseCompletedFlag : 0U,
      pending);
}

template <typename Storage>
inline bool saveCurrentCalibrationEepromIfRequested(
    Storage& storage, const float (&factors)[CURRENT_CALIBRATION_COUNT],
    float baseFactor, uint8_t& pending, bool baseCalibrationCompleted = true) {
  return saveCalibrationEepromRecordIfRequested<
      Storage, CurrentCalibrationEepromRecord>(
      storage, kCurrentCalibrationEepromOffset, kCurrentCalibrationMagic,
      factors, baseFactor,
      baseCalibrationCompleted ? kCalibrationBaseCompletedFlag : 0U,
      pending);
}

template <typename Storage>
inline CalibrationEepromTransactionResult
saveVoltageAndCurrentCalibrationEepromIfRequested(
    Storage& storage,
    const float (&voltageFactors)[VOLTAGE_CALIBRATION_COUNT],
    float voltageBase, uint8_t& voltagePending,
    const float (&currentFactors)[CURRENT_CALIBRATION_COUNT],
    float currentBase, uint8_t& currentPending,
    bool voltageBaseCalibrationCompleted = true,
    bool currentBaseCalibrationCompleted = true) {
  if (!stageCalibrationEepromRecordIfRequested<
          Storage, VoltageCalibrationEepromRecord>(
          storage, kVoltageCalibrationEepromOffset, kVoltageCalibrationMagic,
          voltageFactors, voltageBase,
          voltageBaseCalibrationCompleted ? kCalibrationBaseCompletedFlag : 0U,
          voltagePending)) {
    return CalibrationEepromTransactionResult::kVoltageRejected;
  }

  const bool currentRequested = currentPending == 1;
  if (currentRequested &&
      !stageCalibrationEepromRecordIfRequested<
          Storage, CurrentCalibrationEepromRecord>(
          storage, kCurrentCalibrationEepromOffset, kCurrentCalibrationMagic,
          currentFactors, currentBase,
          currentBaseCalibrationCompleted ? kCalibrationBaseCompletedFlag : 0U,
          currentPending)) {
    return CalibrationEepromTransactionResult::kCurrentRejected;
  }

  if (!storage.commit()) return CalibrationEepromTransactionResult::kCommitFailed;
  voltagePending = 0;
  if (currentRequested) currentPending = 0;
  return CalibrationEepromTransactionResult::kSaved;
}

#if defined(ARDUINO_ARCH_STM32)

class Stm32EepromStorage {
 public:
  bool begin() {
    if (capacity() < kCalibrationEepromCapacity) return false;
    eeprom_buffer_fill();
    ready_ = true;
    return true;
  }

  size_t capacity() const {
    return static_cast<size_t>(E2END) + 1U;
  }

  bool readBytes(size_t offset, void* destination, size_t length) {
    if (!ready_ || offset > capacity() ||
        length > capacity() - offset) {
      return false;
    }
    uint8_t* bytes = static_cast<uint8_t*>(destination);
    for (size_t i = 0; i < length; ++i) {
      bytes[i] = eeprom_buffered_read_byte(
          static_cast<uint32_t>(offset + i));
    }
    return true;
  }

  bool writeBytes(size_t offset, const void* source, size_t length) {
    if (!ready_ || offset > capacity() ||
        length > capacity() - offset) {
      return false;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(source);
    for (size_t i = 0; i < length; ++i) {
      eeprom_buffered_write_byte(
          static_cast<uint32_t>(offset + i), bytes[i]);
    }
    return true;
  }

  bool commit() {
    if (!ready_) return false;
    eeprom_buffer_flush();
    return true;
  }

 private:
  bool ready_ = false;
};

#else

struct CalibrationNullStorage {
  bool begin() { return false; }
  size_t capacity() const { return 0U; }
  bool readBytes(size_t, void*, size_t) { return false; }
  bool writeBytes(size_t, const void*, size_t) { return false; }
  bool commit() { return false; }
};

#endif


constexpr uint32_t CALIBRATION_EEPROM_IDLE_BEFORE_SAVE_MS = 30000U;
constexpr uint32_t CALIBRATION_EEPROM_RETRY_INTERVAL_MS = 1000U;

struct CalibrationLoadReport {
  bool storageReady = false;
  bool voltageLoaded = false;
  bool currentLoaded = false;
  bool voltageMigrated = false;
  bool currentMigrated = false;
};

struct CalibrationResetReport {
  bool currentInitialSaveFailed = false;
  bool voltageInitialSaveFailed = false;
};

struct CalibrationWriteReport {
  bool voltageUnavailable = false;
  bool voltageFailed = false;
  bool voltageSaved = false;
  bool currentUnavailable = false;
  bool currentFailed = false;
  bool currentSaved = false;
};

template <typename Storage>
class CalibrationController {
 public:
  CalibrationController() {
    fillVoltageCalibrationDefaults(voltageFactors_);
    fillCurrentCalibrationDefaults(currentFactors_);
  }

  CalibrationLoadReport beginStorage() {
    fillVoltageCalibrationDefaults(voltageFactors_);
    fillCurrentCalibrationDefaults(currentFactors_);
    voltageBaseFactor_ = VOLTAGE_CALIBRATION_FACTOR_DEFAULT;
    currentBaseFactor_ = CURRENT_CALIBRATION_FACTOR_DEFAULT;
    voltageCalibrationWritePending_ = 1;
    currentCalibrationWritePending_ = 1;
    firstVoltageCalibrationPending_ = false;
    firstCurrentCalibrationPending_ = false;
    voltageBaseCalibrationCompleted_ = true;
    currentBaseCalibrationCompleted_ = false;
    currentStartupWritePending_ = true;
    voltageRetryPending_ = false;
    currentRetryPending_ = false;
    lastEepromWriteAttemptMs_ = 0;
    lastCurrentAttemptMs_ = 0;

    CalibrationLoadReport report;
    report.storageReady = storage_.begin();
    storageReady_ = report.storageReady;
    if (!storageReady_) return report;

    bool voltageCompleted = true;
    const CalibrationEepromLoadResult voltageResult =
        loadVoltageCalibrationEeprom(
            storage_, voltageFactors_, voltageBaseFactor_, voltageCompleted);
    if (voltageResult != CalibrationEepromLoadResult::kInvalid) {
      report.voltageLoaded = true;
      report.voltageMigrated =
          voltageResult == CalibrationEepromLoadResult::kLoadedV1;
      voltageBaseCalibrationCompleted_ = voltageCompleted;
      firstVoltageCalibrationPending_ = !voltageCompleted;
      voltageCalibrationWritePending_ = report.voltageMigrated ? 1 : 0;
    }

    bool currentCompleted = false;
    const CalibrationEepromLoadResult currentResult =
        loadCurrentCalibrationEeprom(
            storage_, currentFactors_, currentBaseFactor_, currentCompleted);
    if (currentResult != CalibrationEepromLoadResult::kInvalid) {
      report.currentLoaded = true;
      report.currentMigrated =
          currentResult == CalibrationEepromLoadResult::kLoadedV1;
      currentBaseCalibrationCompleted_ = currentCompleted;
      currentCalibrationWritePending_ = report.currentMigrated ? 1 : 0;
    }
    return report;
  }

  bool storageReady() const { return storageReady_; }

  float voltageBaseFactor() const { return voltageBaseFactor_; }
  float currentBaseFactor() const { return currentBaseFactor_; }

  const float (&voltageFactors() const)[VOLTAGE_CALIBRATION_COUNT] {
    return voltageFactors_;
  }
  const float (&currentFactors() const)[CURRENT_CALIBRATION_COUNT] {
    return currentFactors_;
  }

  float& mutableVoltageBaseFactor() { return voltageBaseFactor_; }
  float& mutableCurrentBaseFactor() { return currentBaseFactor_; }
  float (&mutableVoltageFactors())[VOLTAGE_CALIBRATION_COUNT] {
    return voltageFactors_;
  }
  float (&mutableCurrentFactors())[CURRENT_CALIBRATION_COUNT] {
    return currentFactors_;
  }
  bool& mutableCurrentBaseCalibrationCompleted() {
    return currentBaseCalibrationCompleted_;
  }
  bool& mutableFirstVoltageCalibrationPending() {
    return firstVoltageCalibrationPending_;
  }
  bool& mutableFirstCurrentCalibrationPending() {
    return firstCurrentCalibrationPending_;
  }

  bool firstVoltageCalibrationPending() const {
    return firstVoltageCalibrationPending_;
  }
  bool firstCurrentCalibrationPending() const {
    return firstCurrentCalibrationPending_;
  }
  bool voltageCalibrationWritePending() const {
    return voltageCalibrationWritePending_ == 1;
  }
  bool currentCalibrationWritePending() const {
    return currentCalibrationWritePending_ == 1;
  }
  bool currentBaseCalibrationCompleted() const {
    return currentBaseCalibrationCompleted_;
  }

  Storage& storageForTest() { return storage_; }

  void noteEncoderActivity(uint32_t nowMs) {
    lastEncoderActivityMs_ = nowMs;
  }

  void enterCurrentCalibration() {
    firstCurrentCalibrationPending_ = !currentBaseCalibrationCompleted_;
  }

  VoltageCalibrationAdjustment adjustVoltage(
      float loadVoltage, bool increase, uint32_t steps = 1) {
    VoltageCalibrationAdjustment result;
    if (!firstVoltageCalibrationPending_ && loadVoltage < 0.0f) return result;

    const float voltageBefore =
        calibrateVoltage(loadVoltage, voltageFactors_);
    int targetDigits = 0;
    float targetVoltage = 0.0f;
    if (!voltageCalibrationDisplayStepTarget(
            voltageBefore, increase, targetDigits, targetVoltage, steps)) {
      return result;
    }

    const size_t index = voltageCalibrationIndex(loadVoltage);
    float& selectedFactor = voltageCalibrationCoefficientForEdit(
        firstVoltageCalibrationPending_, index, voltageBaseFactor_,
        voltageFactors_);
    const float previous = selectedFactor;
    float updated = previous;
    if (firstVoltageCalibrationPending_) {
      if (!solveVoltageBaseFactorForTarget(
              loadVoltage, previous, targetVoltage, voltageFactors_, updated)) {
        return result;
      }
    } else {
      const float coefficientWeight =
          voltageCalibrationCoefficientWeight(loadVoltage, index);
      const float voltagePerCoefficient = loadVoltage * coefficientWeight;
      if (!(voltagePerCoefficient > 0.0f)) return result;
      updated += (targetVoltage - voltageBefore) / voltagePerCoefficient;
    }
    if (!(updated > 0.0f && updated <= FLT_MAX)) return result;

    selectedFactor = updated;
    const float calibrationInput = firstVoltageCalibrationPending_
        ? rescaleVoltageAfterBaseFactorChange(
              loadVoltage, previous, updated)
        : loadVoltage;
    const float corrected = calibrateVoltage(
        calibrationInput, voltageFactors_);
    if (!(corrected >= 0.0f && corrected < 2147483.0f) ||
        voltageDisplayDigits(corrected) != targetDigits) {
      selectedFactor = previous;
      return result;
    }

    if (firstVoltageCalibrationPending_) {
      loadVoltage = calibrationInput;
    }
    result.changed = true;
    result.correctedVoltage = corrected;
    result.adjustedLoadVoltage = loadVoltage;
    result.displayDigits = targetDigits;
    voltageCalibrationWritePending_ = 1;
    return result;
  }

  CurrentCalibrationAdjustment adjustCurrent(
      float measuredCurrent, bool increase, uint32_t steps = 1) {
    CurrentCalibrationAdjustment result;
    const float measured = measuredCurrent < 0.0f
        ? -measuredCurrent : measuredCurrent;
    const float rawCurrent = measured * currentBaseFactor_;
    const float currentBefore =
        calibrateCurrent(rawCurrent, currentFactors_);
    int targetDigits = 0;
    float targetCurrent = 0.0f;
    if (!currentCalibrationDisplayStepTarget(
            currentBefore, firstCurrentCalibrationPending_, increase,
            targetDigits, targetCurrent, steps)) {
      return result;
    }

    const size_t index = currentCalibrationIndex(rawCurrent);
    float& selectedFactor = currentCalibrationCoefficientForEdit(
        firstCurrentCalibrationPending_, index, currentBaseFactor_,
        currentFactors_);
    const float previous = selectedFactor;
    float updated = previous;
    if (firstCurrentCalibrationPending_) {
      if (!solveCurrentBaseFactorForTarget(
              measured, previous, targetCurrent, currentFactors_, updated)) {
        return result;
      }
    } else {
      const float coefficientWeight =
          currentCalibrationCoefficientWeight(rawCurrent, index);
      const float currentPerCoefficient = rawCurrent * coefficientWeight;
      if (!(currentPerCoefficient > 0.0f)) return result;
      updated += (targetCurrent - currentBefore) / currentPerCoefficient;
    }
    if (!(updated > 0.0f && updated <= FLT_MAX)) return result;

    selectedFactor = updated;
    const float calibrationInput = measured * currentBaseFactor_;
    const float corrected = calibrateCurrent(
        calibrationInput, currentFactors_);
    if (!(corrected >= 0.0f && corrected < 2147483.0f) ||
        currentDisplayDigits(
            corrected, firstCurrentCalibrationPending_) != targetDigits) {
      selectedFactor = previous;
      return result;
    }

    result.changed = true;
    result.correctedCurrent = corrected;
    result.displayDigits = targetDigits;
    currentCalibrationWritePending_ = 1;
    return result;
  }

  CalibrationResetReport beginFirstVoltageCalibration() {
    CalibrationResetReport report;
    currentBaseFactor_ = CURRENT_CALIBRATION_FACTOR_DEFAULT;
    fillCurrentCalibrationDefaults(currentFactors_);
    currentBaseCalibrationCompleted_ = false;
    currentCalibrationWritePending_ = 1;
    report.currentInitialSaveFailed =
        !storageReady_ ||
        !saveCurrentCalibrationEepromIfRequested(
            storage_, currentFactors_, currentBaseFactor_,
            currentCalibrationWritePending_, false);

    fillVoltageCalibrationDefaults(voltageFactors_);
    voltageBaseFactor_ = VOLTAGE_CALIBRATION_FACTOR_DEFAULT;
    voltageBaseCalibrationCompleted_ = false;
    voltageCalibrationWritePending_ = 1;
    report.voltageInitialSaveFailed =
        !storageReady_ ||
        !saveVoltageCalibrationEepromIfRequested(
            storage_, voltageFactors_, voltageBaseFactor_,
            voltageCalibrationWritePending_, false);

    firstVoltageCalibrationPending_ = true;
    firstCurrentCalibrationPending_ = false;
    return report;
  }

  CalibrationEepromTransactionResult finishFirstVoltageCalibration() {
    voltageCalibrationWritePending_ = 1;
    const CalibrationEepromTransactionResult result =
        !storageReady_
            ? CalibrationEepromTransactionResult::kVoltageRejected
            : saveVoltageAndCurrentCalibrationEepromIfRequested(
                  storage_, voltageFactors_, voltageBaseFactor_,
                  voltageCalibrationWritePending_, currentFactors_,
                  currentBaseFactor_, currentCalibrationWritePending_,
                  true, currentBaseCalibrationCompleted_);
    if (result == CalibrationEepromTransactionResult::kSaved) {
      voltageBaseCalibrationCompleted_ = true;
      firstVoltageCalibrationPending_ = false;
    }
    return result;
  }

  bool finishFirstCurrentCalibration() {
    currentCalibrationWritePending_ = 1;
    if (!storageReady_ ||
        !saveCurrentCalibrationEepromIfRequested(
            storage_, currentFactors_, currentBaseFactor_,
            currentCalibrationWritePending_, true)) {
      return false;
    }
    currentBaseCalibrationCompleted_ = true;
    firstCurrentCalibrationPending_ = false;
    return true;
  }

  CalibrationWriteReport processWrites(uint32_t nowMs) {
    CalibrationWriteReport report;
    processVoltageWrite(nowMs, report);
    processCurrentWrite(nowMs, report);
    return report;
  }

 private:
  void processVoltageWrite(
      uint32_t nowMs, CalibrationWriteReport& report) {
    if (firstVoltageCalibrationPending_) return;
    if (voltageCalibrationWritePending_ != 1) {
      voltageRetryPending_ = false;
      return;
    }
    if (static_cast<uint32_t>(nowMs - lastEncoderActivityMs_) <=
        CALIBRATION_EEPROM_IDLE_BEFORE_SAVE_MS) {
      return;
    }
    if (voltageRetryPending_ &&
        static_cast<uint32_t>(nowMs - lastEepromWriteAttemptMs_) <
            CALIBRATION_EEPROM_RETRY_INTERVAL_MS) {
      return;
    }

    lastEepromWriteAttemptMs_ = nowMs;
    if (!storageReady_) {
      voltageRetryPending_ = true;
      report.voltageUnavailable = true;
      return;
    }

    const bool saved = saveVoltageCalibrationEepromIfRequested(
        storage_, voltageFactors_, voltageBaseFactor_,
        voltageCalibrationWritePending_,
        voltageBaseCalibrationCompleted_);
    voltageRetryPending_ = !saved;
    if (saved) report.voltageSaved = true;
    else report.voltageFailed = true;
  }

  void processCurrentWrite(
      uint32_t nowMs, CalibrationWriteReport& report) {
    if (firstCurrentCalibrationPending_) return;
    if (currentCalibrationWritePending_ != 1) {
      currentRetryPending_ = false;
      currentStartupWritePending_ = false;
      return;
    }
    const bool startupWrite = currentStartupWritePending_;
    if (!startupWrite &&
        static_cast<uint32_t>(nowMs - lastEncoderActivityMs_) <=
            CALIBRATION_EEPROM_IDLE_BEFORE_SAVE_MS) {
      return;
    }
    currentStartupWritePending_ = false;
    if (currentRetryPending_ &&
        static_cast<uint32_t>(nowMs - lastCurrentAttemptMs_) <
            CALIBRATION_EEPROM_RETRY_INTERVAL_MS) {
      return;
    }

    lastCurrentAttemptMs_ = nowMs;
    if (!storageReady_) {
      currentRetryPending_ = true;
      report.currentUnavailable = true;
      return;
    }

    const bool saved = saveCurrentCalibrationEepromIfRequested(
        storage_, currentFactors_, currentBaseFactor_,
        currentCalibrationWritePending_,
        currentBaseCalibrationCompleted_);
    currentRetryPending_ = !saved;
    if (saved) report.currentSaved = true;
    else report.currentFailed = true;
  }

  Storage storage_{};
  float voltageBaseFactor_ = VOLTAGE_CALIBRATION_FACTOR_DEFAULT;
  float currentBaseFactor_ = CURRENT_CALIBRATION_FACTOR_DEFAULT;
  float voltageFactors_[VOLTAGE_CALIBRATION_COUNT] = {};
  float currentFactors_[CURRENT_CALIBRATION_COUNT] = {};
  bool firstVoltageCalibrationPending_ = false;
  bool firstCurrentCalibrationPending_ = false;
  bool voltageBaseCalibrationCompleted_ = true;
  bool currentBaseCalibrationCompleted_ = false;
  uint8_t voltageCalibrationWritePending_ = 0;
  uint8_t currentCalibrationWritePending_ = 0;
  bool storageReady_ = false;
  uint32_t lastEncoderActivityMs_ = 0;
  uint32_t lastEepromWriteAttemptMs_ = 0;
  uint32_t lastCurrentAttemptMs_ = 0;
  bool voltageRetryPending_ = false;
  bool currentRetryPending_ = false;
  bool currentStartupWritePending_ = true;
};

#if defined(ARDUINO_ARCH_STM32)
using ApplicationCalibrationStorage = Stm32EepromStorage;
#else
using ApplicationCalibrationStorage = CalibrationNullStorage;
#endif
using ApplicationCalibrationController =
    CalibrationController<ApplicationCalibrationStorage>;
