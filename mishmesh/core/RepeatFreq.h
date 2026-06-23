#pragma once

#include <stdint.h>

namespace mishmesh {

// Band bucket for a frequency in MHz (mirrors the companion app's mapping):
// 0 = 433 band, 1 = 868/869 band, 2 = 915/918 band.
inline int repeatBandIndex(float mhz) {
  if (mhz < 500.0f) return 0;
  if (mhz < 900.0f) return 1;
  return 2;
}

// From a list of allowed off-grid repeat frequencies (kHz, from the firmware's
// repeat_freq_ranges), pick the one in the same band as curMhz. Returns kHz,
// or 0 if the current band has no allowed off-grid frequency.
inline uint32_t pickRepeatFreqKhz(float curMhz, const uint32_t* allowedKhz, int n) {
  if (!allowedKhz || n <= 0) return 0;
  int band = repeatBandIndex(curMhz);
  for (int i = 0; i < n; i++)
    if (repeatBandIndex((float)allowedKhz[i] / 1000.0f) == band) return allowedKhz[i];
  return 0;
}

}  // namespace mishmesh
