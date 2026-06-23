#pragma once

#include <stdint.h>

namespace mishmesh {

// Screen auto-off options, in stepper order. Each index maps 1:1 to a label and
// a millisecond timeout; the last option ("Never") maps to 0 = auto-off
// disabled (AppletHost::setAutoOffMillis treats 0 as "stay on").
static const int SCREEN_SLEEP_COUNT = 6;

inline uint32_t screenSleepMillis(int idx) {
  static const uint32_t MS[SCREEN_SLEEP_COUNT] =
      { 15000u, 30000u, 60000u, 120000u, 300000u, 0u };
  return (idx >= 0 && idx < SCREEN_SLEEP_COUNT) ? MS[idx] : 30000u;
}

inline const char* screenSleepLabel(int idx) {
  static const char* const L[SCREEN_SLEEP_COUNT] =
      { "15s", "30s", "1m", "2m", "5m", "Never" };
  return (idx >= 0 && idx < SCREEN_SLEEP_COUNT) ? L[idx] : "30s";
}

// NodePrefs stores index+1 so a zeroed/legacy prefs byte (0) resolves to the
// 30s default rather than 15s. Encode on write, decode on read.
inline int screenSleepStoredToIndex(uint8_t stored) {
  if (stored == 0) return 1;                       // unset/legacy -> 30s
  int idx = (int)stored - 1;
  return (idx >= 0 && idx < SCREEN_SLEEP_COUNT) ? idx : 1;
}

inline uint8_t screenSleepIndexToStored(int idx) {
  if (idx < 0 || idx >= SCREEN_SLEEP_COUNT) idx = 1;   // default 30s
  return (uint8_t)(idx + 1);
}

}  // namespace mishmesh
