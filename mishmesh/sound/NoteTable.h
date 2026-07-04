#pragma once
#include <stdint.h>

namespace mishmesh { namespace sound {

// Equal-tempered octave-4 frequencies, semitone 0=C .. 11=B.
// Defined once in ScoreSource.cpp; declared extern so the table isn't emitted
// per-including-TU (it was duplicated across ScoreSource + RtttlSource).
extern const uint16_t kOctave4Hz[12];

// Frequency for a semitone (0..11) at an octave offset relative to octave 4.
// Each octave up doubles the frequency, so a bit-shift is exact enough for a
// square-wave piezo.
inline uint16_t noteHz(uint8_t semitone, int octaveShift) {
  uint32_t f = kOctave4Hz[semitone % 12];
  if (octaveShift >= 0) f <<= octaveShift; else f >>= (-octaveShift);
  return (uint16_t)f;
}

}}  // namespace mishmesh::sound
