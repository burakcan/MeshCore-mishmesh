#pragma once
#include <stdint.h>
#include <mishmesh/core/InputEvent.h>

namespace mishmesh { namespace arduboy {

// Host-safe (no Arduino) helper: the 60Hz nextFrame-style sim gate + the held->Arduboy
// button mapping + a one-shot A/B latch. Bit values mirror Arduboy2's button bits.
class ArduboyGate {
public:
  static const uint8_t LEFT  = 0x20, RIGHT = 0x40, UP = 0x80, DOWN = 0x10, A_B = 0x0C;
  static const uint32_t FRAME_MS = 16;   // ~60Hz (1000/60 truncated, matches the framework cap)

  ArduboyGate() : _last(0), _started(false), _ab(false), _frameMs(FRAME_MS) {}

  // [mishmesh] Override the step interval (ms). 0 restores the ~60Hz default.
  void setFrameMs(uint32_t ms) { _frameMs = ms ? ms : FRAME_MS; }

  // True at most once per _frameMs; resets the timer to `now` (no multi-step catch-up).
  bool stepDue(uint32_t now) {
    if (!_started) { _started = true; _last = now; return true; }
    if ((uint32_t)(now - _last) >= _frameMs) { _last = now; return true; }
    return false;
  }
  void pressAB() { _ab = true; }
  // Arduboy button mask: held directions from InputState OR a one-shot A/B (clears on read).
  uint8_t buttonMask(const InputState& in) {
    uint8_t m = 0;
    if (in.isDown(InputEvent::NavLeft))  m |= LEFT;
    if (in.isDown(InputEvent::NavRight)) m |= RIGHT;
    if (in.isDown(InputEvent::NavUp))    m |= UP;
    if (in.isDown(InputEvent::NavDown))  m |= DOWN;
    if (_ab) { m |= A_B; _ab = false; }
    return m;
  }
private:
  uint32_t _last; bool _started; bool _ab;
  uint32_t _frameMs;   // [mishmesh] configurable step interval (default FRAME_MS)
};

}}  // namespace mishmesh::arduboy
