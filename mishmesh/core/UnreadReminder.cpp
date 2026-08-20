// mishmesh/core/UnreadReminder.cpp
#include "UnreadReminder.h"

namespace mishmesh {

void UnreadReminder::configure(uint16_t intervalSec, uint16_t stopAfterSec) {
  _intervalMs = (uint32_t)intervalSec * 1000;
  _stopMs = (uint32_t)stopAfterSec * 1000;
}

void UnreadReminder::noteArrival(uint32_t now) {
  _armedAt = _lastBeep = now;   // the arrival tone counts as this cycle's beep
  _armed = true;
}

bool UnreadReminder::tick(uint32_t now, uint16_t notifyUnread, uint32_t lastInputMs) {
  if (!_armed) return false;
  if (notifyUnread == 0) { _armed = false; return false; }
  // Signed difference so a millis() wrap doesn't read as "input in the future".
  if ((int32_t)(lastInputMs - _armedAt) > 0) { _armed = false; return false; }
  if (_stopMs && now - _armedAt >= _stopMs) { _armed = false; return false; }
  if (_intervalMs == 0) return false;
  if (now - _lastBeep < _intervalMs) return false;
  _lastBeep = now;
  return true;
}

}  // namespace mishmesh
