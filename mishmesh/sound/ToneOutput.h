#pragma once
#include <stdint.h>

namespace mishmesh { namespace sound {

// Volume = PWM duty cycle on the passive piezo (Mute = pin idle).
enum class VolumeLevel : uint8_t { Mute = 0, Low, Mid, High };

// Owns the buzzer pin. Exactly one concrete impl is compiled per firmware env.
struct IToneOutput {
  virtual ~IToneOutput() {}
  virtual void begin() = 0;
  virtual void tone(uint16_t freqHz, VolumeLevel vol) = 0;  // set pitch + volume
  virtual void silence() = 0;
};

// Provided by the platform output .cpp linked into the firmware (device-only).
IToneOutput* defaultToneOutput();

}}  // namespace mishmesh::sound
