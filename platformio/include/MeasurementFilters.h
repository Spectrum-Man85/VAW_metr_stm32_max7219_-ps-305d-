#pragma once

#include <stddef.h>
#include <stdint.h>

namespace MeasurementFilters {

constexpr int DISPLAY_HYSTERESIS_COUNTS = 2;

template <typename T, size_t N>
constexpr size_t countValuesLessThan(const T (&values)[N], T candidate,
                                     size_t index = 0) {
  return index >= N
      ? 0U
      : (values[index] < candidate ? 1U : 0U) +
            countValuesLessThan(values, candidate, index + 1U);
}

template <typename T, size_t N>
constexpr size_t countValuesLessThanOrEqual(const T (&values)[N], T candidate,
                                            size_t index = 0) {
  return index >= N
      ? 0U
      : (values[index] <= candidate ? 1U : 0U) +
            countValuesLessThanOrEqual(values, candidate, index + 1U);
}

template <typename T, size_t N>
constexpr T findMedianCandidate(const T (&values)[N], size_t index = 0) {
  return index >= N
      ? values[0]
      : (countValuesLessThan(values, values[index]) <= N / 2U &&
                 countValuesLessThanOrEqual(values, values[index]) > N / 2U
             ? values[index]
             : findMedianCandidate(values, index + 1U));
}

template <typename T, size_t N>
struct MedianWindow {
  static_assert(N > 0, "Median window must contain at least one value");
  static_assert(N % 2U == 1U, "Median window size must be odd");

  static constexpr T calculate(const T (&values)[N]) {
    return findMedianCandidate(values);
  }
};

template <typename T, size_t N>
constexpr T medianOfWindow(const T (&values)[N]) {
  return MedianWindow<T, N>::calculate(values);
}

template <typename T, size_t N>
class MedianFilter {
 public:
  void reset(T sample) {
    for (size_t i = 0; i < N; ++i) {
      samples_[i] = sample;
    }
    next_ = 0U;
    initialized_ = true;
  }

  T add(T sample) {
    if (!initialized_) {
      reset(sample);
    } else {
      samples_[next_] = sample;
      next_ = (next_ + 1U) % N;
    }

    return medianOfWindow(samples_);
  }

 private:
  T samples_[N] = {};
  size_t next_ = 0U;
  bool initialized_ = false;
};

constexpr uint32_t displayedValueDifference(int displayed, int candidate) {
  return displayed > candidate
      ? static_cast<uint32_t>(displayed) - static_cast<uint32_t>(candidate)
      : static_cast<uint32_t>(candidate) - static_cast<uint32_t>(displayed);
}

constexpr bool shouldChangeDisplayedValue(int displayed, int candidate) {
  return displayedValueDifference(displayed, candidate) >
         static_cast<uint32_t>(DISPLAY_HYSTERESIS_COUNTS);
}

enum class FilterMode : uint8_t {
  kSlow,
  kMedium,
  kFast,
  kInstant
};

template <typename FastFilter, typename MediumFilter, typename SlowFilter>
void synchronizeAdaptiveFilters(
    FilterMode mode, float rawValue, float selectedValue,
    FastFilter& fastFilter, MediumFilter& mediumFilter,
    SlowFilter& slowFilter) {
  switch (mode) {
    case FilterMode::kInstant:
      fastFilter.reset(rawValue);
      mediumFilter.reset(rawValue);
      slowFilter.reset(rawValue);
      break;
    case FilterMode::kFast:
      mediumFilter.reset(selectedValue);
      slowFilter.reset(selectedValue);
      break;
    case FilterMode::kMedium:
      slowFilter.reset(selectedValue);
      break;
    case FilterMode::kSlow:
      break;
  }
}

constexpr float changeMagnitude(float displayedValue, float currentValue) {
  const float difference = currentValue - displayedValue;
  return difference < 0.0f ? -difference : difference;
}

constexpr FilterMode selectFilterForChange(
    float change, float slowChangeThreshold,
    float mediumChangeThreshold, float fastChangeThreshold) {
  if (!(change >= 0.0f)) return FilterMode::kSlow;
  if (change < slowChangeThreshold) return FilterMode::kSlow;
  if (change < mediumChangeThreshold) return FilterMode::kMedium;
  if (change < fastChangeThreshold) return FilterMode::kFast;
  return FilterMode::kInstant;
}

constexpr FilterMode selectFilter(
    float displayedValue, float currentValue,
    float slowChangeThreshold, float mediumChangeThreshold,
    float fastChangeThreshold) {
  return selectFilterForChange(
      changeMagnitude(displayedValue, currentValue),
      slowChangeThreshold, mediumChangeThreshold, fastChangeThreshold);
}

constexpr uint8_t filterDisplayNumber(FilterMode mode) {
  switch (mode) {
    case FilterMode::kInstant:
      return 0U;
    case FilterMode::kFast:
      return 1U;
    case FilterMode::kMedium:
      return 2U;
    case FilterMode::kSlow:
      return 3U;
  }
  return 3U;
}

constexpr bool filterIndicatorNeedsRedraw(
    int previousDisplayNumber, FilterMode mode, bool modeScreenRedrawn) {
  return modeScreenRedrawn ||
      previousDisplayNumber !=
          static_cast<int>(filterDisplayNumber(mode));
}

struct ChannelResult {
  float value;
  FilterMode mode;
};

struct FrameResult {
  ChannelResult voltage;
  ChannelResult current;
};

namespace Detail {

constexpr size_t kFastFilterSize = 11U;
constexpr size_t kMediumFilterSize = 21U;
constexpr size_t kSlowFilterSize = 41U;

struct FilterThresholds {
  float slow;
  float medium;
  float fast;
};

constexpr FilterThresholds kVoltageThresholds = {0.01f, 0.05f, 0.1f};
constexpr FilterThresholds kCurrentThresholds = {0.01f, 0.05f, 0.1f};

struct ChannelFilterBank {
  MedianFilter<float, kFastFilterSize> fast;
  MedianFilter<float, kMediumFilterSize> medium;
  MedianFilter<float, kSlowFilterSize> slow;

  void reset() {
    fast = MedianFilter<float, kFastFilterSize>();
    medium = MedianFilter<float, kMediumFilterSize>();
    slow = MedianFilter<float, kSlowFilterSize>();
  }
};

struct FilterBank {
  ChannelFilterBank voltage;
  ChannelFilterBank current;
};

inline FilterBank& filterBank() {
  static FilterBank bank;
  return bank;
}

inline ChannelResult processChannel(
    float rawValue, int previousDisplayedDigits, bool adaptiveEnabled,
    ChannelFilterBank& filters, const FilterThresholds& thresholds) {
  const float previousDisplayedValue =
      previousDisplayedDigits < 0
          ? rawValue
          : static_cast<float>(previousDisplayedDigits) * 0.001f;

  float filteredFast = rawValue;
  float filteredMedium = rawValue;
  float filteredSlow = rawValue;
  if (adaptiveEnabled) {
    filteredFast = filters.fast.add(rawValue);
    filteredMedium = filters.medium.add(rawValue);
    filteredSlow = filters.slow.add(rawValue);
  }

  const FilterMode mode =
      adaptiveEnabled
          ? selectFilter(previousDisplayedValue, rawValue,
                         thresholds.slow, thresholds.medium, thresholds.fast)
          : FilterMode::kInstant;

  float selectedValue = rawValue;
  if (mode == FilterMode::kSlow) {
    selectedValue = filteredSlow;
  } else if (mode == FilterMode::kMedium) {
    selectedValue = filteredMedium;
  } else if (mode == FilterMode::kFast) {
    selectedValue = filteredFast;
  }

  synchronizeAdaptiveFilters(
      mode, rawValue, selectedValue,
      filters.fast, filters.medium, filters.slow);
  return {selectedValue, mode};
}

}  // namespace Detail

inline FrameResult process(float rawVoltage, float rawCurrent,
                           int previousVoltageDigits,
                           int previousCurrentDigits,
                           bool adaptiveEnabled) {
  Detail::FilterBank& bank = Detail::filterBank();
  return {
      Detail::processChannel(rawVoltage, previousVoltageDigits, adaptiveEnabled,
                             bank.voltage, Detail::kVoltageThresholds),
      Detail::processChannel(rawCurrent, previousCurrentDigits, adaptiveEnabled,
                             bank.current, Detail::kCurrentThresholds)};
}

inline void resetAll() {
  Detail::FilterBank& bank = Detail::filterBank();
  bank.voltage.reset();
  bank.current.reset();
}

inline void resetVoltage() {
  Detail::filterBank().voltage.reset();
}

inline void resetCurrent() {
  Detail::filterBank().current.reset();
}

}  // namespace MeasurementFilters