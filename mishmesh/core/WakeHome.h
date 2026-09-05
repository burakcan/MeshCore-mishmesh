#pragma once

#include <stdint.h>

namespace mishmesh {

// How long the panel has to have been asleep before a wake resets navigation to
// the home face. This follows the watch convention rather than the phone one: a
// phone never discards where you were, but home here is a status face, so coming
// back to it after a real absence is what you want. Options in stepper order.
static const int WAKE_HOME_COUNT = 5;
static const uint32_t WAKE_HOME_NEVER = 0xFFFFFFFFu;

inline uint32_t wakeHomeMillis(int idx) {
  static const uint32_t MS[WAKE_HOME_COUNT] =
      { 0u, 60000u, 120000u, 300000u, WAKE_HOME_NEVER };
  return (idx >= 0 && idx < WAKE_HOME_COUNT) ? MS[idx] : 120000u;
}

inline const char* wakeHomeLabel(int idx) {
  static const char* const L[WAKE_HOME_COUNT] =
      { "Always", "1m", "2m", "5m", "Never" };
  return (idx >= 0 && idx < WAKE_HOME_COUNT) ? L[idx] : "2m";
}

// NodePrefs stores index+1 so a zeroed/legacy prefs byte resolves to the 2m
// default rather than to "Always". Encode on write, decode on read.
inline int wakeHomeStoredToIndex(uint8_t stored) {
  if (stored == 0) return 2;                       // unset/legacy -> 2m
  int idx = (int)stored - 1;
  return (idx >= 0 && idx < WAKE_HOME_COUNT) ? idx : 2;
}

inline uint8_t wakeHomeIndexToStored(int idx) {
  if (idx < 0 || idx >= WAKE_HOME_COUNT) idx = 2;
  return (uint8_t)(idx + 1);
}

}  // namespace mishmesh
