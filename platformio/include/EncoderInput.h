#pragma once

#include <stdint.h>

enum class EncoderEvent : int8_t {
  None = 0,
  Right = 1,
  Left = -1
};

constexpr int8_t quadratureDelta(uint8_t previousState, uint8_t currentState) {
  const uint8_t transition =
      static_cast<uint8_t>((previousState << 2U) | currentState);
  return transition == 0x01U || transition == 0x07U ||
                 transition == 0x08U || transition == 0x0EU
             ? 1
             : (transition == 0x02U || transition == 0x04U ||
                        transition == 0x0BU || transition == 0x0DU
                    ? -1
                    : 0);
}

constexpr bool hasDebounceElapsed(uint32_t nowMs, uint32_t changedAtMs,
                                  uint32_t debounceMs) {
  return static_cast<uint32_t>(nowMs - changedAtMs) >= debounceMs;
}

class EncoderDecoder {
 public:
  void initialize(bool channelA, bool channelB) {
    previousState_ = makeState(channelA, channelB);
    accumulator_ = 0;
  }

  EncoderEvent update(bool channelA, bool channelB) {
    const uint8_t currentState = makeState(channelA, channelB);
    if (currentState == previousState_) {
      return EncoderEvent::None;
    }

    const int8_t delta = quadratureDelta(previousState_, currentState);
    previousState_ = currentState;
    if (delta == 0) {
      accumulator_ = 0;
      return EncoderEvent::None;
    }

    accumulator_ += delta;
    if (accumulator_ >= 4) {
      accumulator_ = 0;
      return EncoderEvent::Right;
    }
    if (accumulator_ <= -4) {
      accumulator_ = 0;
      return EncoderEvent::Left;
    }
    return EncoderEvent::None;
  }

 private:
  static uint8_t makeState(bool channelA, bool channelB) {
    return static_cast<uint8_t>((channelA ? 2U : 0U) |
                                (channelB ? 1U : 0U));
  }

  uint8_t previousState_ = 0;
  int8_t accumulator_ = 0;
};

class EncoderEventQueue {
 public:
  static constexpr uint8_t kCapacity = 64U;

  bool pushFromIsr(EncoderEvent event) {
    if (event == EncoderEvent::None) return false;
    const uint8_t next = static_cast<uint8_t>((head_ + 1U) % kCapacity);
    if (next == tail_) {
      ++dropped_;
      return false;
    }
    events_[head_] = event;
    head_ = next;
    return true;
  }

  bool pop(EncoderEvent& event) {
    if (tail_ == head_) return false;
    event = events_[tail_];
    tail_ = static_cast<uint8_t>((tail_ + 1U) % kCapacity);
    return true;
  }

  uint32_t dropped() const { return dropped_; }

 private:
  volatile EncoderEvent events_[kCapacity] = {};
  volatile uint8_t head_ = 0;
  volatile uint8_t tail_ = 0;
  volatile uint32_t dropped_ = 0;
};

// Acceleration counts complete detents, never raw quadrature transitions.
class EncoderAcceleration {
 public:
  explicit EncoderAcceleration(uint32_t maxMultiplier)
      : maxMultiplier_(maxMultiplier == 0 ? 1 : maxMultiplier) {}

  uint32_t onDetent(bool increase) {
    const int8_t direction = increase ? 1 : -1;
    if (direction != lastDirection_) {
      count_ = 0;
      lastDirection_ = direction;
      multiplier_ = 1;
    }
    if (++count_ == 10) {
      count_ = 0;
      if (multiplier_ < maxMultiplier_) {
        multiplier_ = multiplier_ <= maxMultiplier_ / 2
            ? multiplier_ * 2
            : maxMultiplier_;
      }
    }
    return multiplier_;
  }

  void reset() {
    multiplier_ = 1;
    lastDirection_ = 0;
    count_ = 0;
  }

 private:
  const uint32_t maxMultiplier_;
  uint32_t multiplier_ = 1;
  int8_t lastDirection_ = 0;
  uint8_t count_ = 0;
};

class DebouncedButton {
 public:
  explicit DebouncedButton(uint32_t debounceMs) : debounceMs_(debounceMs) {}

  void initialize(bool rawPressed) {
    candidatePressed_ = rawPressed;
    stablePressed_ = rawPressed;
    candidateChangedAtMs_ = 0;
  }

  bool update(bool rawPressed, uint32_t nowMs) {
    if (rawPressed != candidatePressed_) {
      candidatePressed_ = rawPressed;
      candidateChangedAtMs_ = nowMs;
      return false;
    }

    if (candidatePressed_ != stablePressed_ &&
        hasDebounceElapsed(nowMs, candidateChangedAtMs_, debounceMs_)) {
      stablePressed_ = candidatePressed_;
      return stablePressed_;
    }
    return false;
  }

 private:
  const uint32_t debounceMs_;
  uint32_t candidateChangedAtMs_ = 0;
  bool candidatePressed_ = false;
  bool stablePressed_ = false;
};

struct EncoderInputSnapshot {
  uint8_t rawState = 0;
  uint32_t interruptEdges = 0;
  uint32_t channelAChanges = 0;
  uint32_t channelBChanges = 0;
  uint32_t invalidTransitions = 0;
  uint32_t droppedEvents = 0;
};

#if defined(ARDUINO)

#include <Arduino.h>

#include "BoardPins.h"

class EncoderInput {
 public:
  static constexpr uint32_t kButtonDebounceMs = 30U;

  void configurePins() {
    pinMode(BoardPins::kEncoderButton, INPUT_PULLUP);
    pinMode(BoardPins::kEncoderA, INPUT_PULLUP);
    pinMode(BoardPins::kEncoderB, INPUT_PULLUP);

    const bool initialButtonPressed = buttonPressed();
    const bool initialA = digitalRead(BoardPins::kEncoderA) != LOW;
    const bool initialB = digitalRead(BoardPins::kEncoderB) != LOW;
    button_.initialize(initialButtonPressed);
    decoder_.initialize(initialA, initialB);
    rawState_ = makeState(initialA, initialB);
    activityPending_ = false;
    buttonPressPending_ = false;
  }

  void startInterrupts() {
    activeInstance() = this;
    attachInterrupt(digitalPinToInterrupt(BoardPins::kEncoderA),
                    interruptThunk, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BoardPins::kEncoderB),
                    interruptThunk, CHANGE);
  }

  bool buttonPressed() const {
    return digitalRead(BoardPins::kEncoderButton) == LOW;
  }

  void poll(uint32_t nowMs) {
    const bool pressed = buttonPressed();
    if (pressed) activityPending_ = true;
    if (button_.update(pressed, nowMs)) {
      buttonPressPending_ = true;
    }
  }

  bool popEvent(EncoderEvent& event) {
    bool available = false;
    noInterrupts();
    available = queue_.pop(event);
    interrupts();
    return available;
  }

  bool consumeActivity() {
    bool active = false;
    noInterrupts();
    active = activityPending_;
    activityPending_ = false;
    interrupts();
    return active;
  }

  bool consumeButtonPress() {
    const bool pressed = buttonPressPending_;
    buttonPressPending_ = false;
    return pressed;
  }

  EncoderInputSnapshot snapshot() const {
    EncoderInputSnapshot result;
    noInterrupts();
    result.rawState = rawState_;
    result.droppedEvents = queue_.dropped();
#if defined(ENCODER_DIAGNOSTICS)
    result.interruptEdges = interruptEdges_;
    result.channelAChanges = channelAChanges_;
    result.channelBChanges = channelBChanges_;
    result.invalidTransitions = invalidTransitions_;
#endif
    interrupts();
    return result;
  }

 private:
  static uint8_t makeState(bool channelA, bool channelB) {
    return static_cast<uint8_t>((channelA ? 2U : 0U) |
                                (channelB ? 1U : 0U));
  }

  static EncoderInput*& activeInstance() {
    static EncoderInput* instance = nullptr;
    return instance;
  }

  static void interruptThunk() {
    EncoderInput* instance = activeInstance();
    if (instance != nullptr) instance->handleInterrupt();
  }

  void handleInterrupt() {
    const bool channelA = digitalRead(BoardPins::kEncoderA) != LOW;
    const bool channelB = digitalRead(BoardPins::kEncoderB) != LOW;
    const uint8_t state = makeState(channelA, channelB);

#if defined(ENCODER_DIAGNOSTICS)
    ++interruptEdges_;
#endif
    const uint8_t changed = state ^ rawState_;
    if (changed == 0U) return;

    rawState_ = state;
    activityPending_ = true;
#if defined(ENCODER_DIAGNOSTICS)
    if (changed & 2U) ++channelAChanges_;
    if (changed & 1U) ++channelBChanges_;
    if (changed == 3U) ++invalidTransitions_;
#endif

    const EncoderEvent event = decoder_.update(channelA, channelB);
    if (event != EncoderEvent::None) {
      queue_.pushFromIsr(event);
    }
  }

  EncoderDecoder decoder_;
  EncoderEventQueue queue_;
  DebouncedButton button_{kButtonDebounceMs};
  volatile uint8_t rawState_ = 0;
  volatile bool activityPending_ = false;
  bool buttonPressPending_ = false;
#if defined(ENCODER_DIAGNOSTICS)
  volatile uint32_t interruptEdges_ = 0;
  volatile uint32_t channelAChanges_ = 0;
  volatile uint32_t channelBChanges_ = 0;
  volatile uint32_t invalidTransitions_ = 0;
#endif
};

#endif
