#pragma once
#include <stdint.h>

namespace mishmesh { namespace sound {

// Equal-tempered octave-4 frequencies, semitone 0=C .. 11=B.
static const uint16_t kOctave4Hz[12] = {
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};

// Frequency for a semitone (0..11) at an octave offset relative to octave 4.
// Each octave up doubles the frequency, so a bit-shift is exact enough for a
// square-wave piezo.
inline uint16_t noteHz(uint8_t semitone, int octaveShift) {
  uint32_t f = kOctave4Hz[semitone % 12];
  if (octaveShift >= 0) f <<= octaveShift; else f >>= (-octaveShift);
  return (uint16_t)f;
}

}}  // namespace mishmesh::sound
