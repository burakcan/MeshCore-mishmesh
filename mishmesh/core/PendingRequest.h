#pragma once

#include <stdint.h>

namespace mishmesh {

// Reusable async-request tracker for the "snapshot a service seq counter, fire the
// request, poll until the counter bumps or a timeout elapses" pattern (the same
// loop ServerLoginApplet runs by hand). No allocation; a plain value member of the
// consuming applet. Times are the host loop clock (AppletHost::nowMs / Canvas::now).
class PendingRequest {
public:
  enum class State : uint8_t { Idle, Waiting, Ready, TimedOut };

  uint32_t timeoutMs = 8000;   // caller may tune before begin()

  // Snapshot the current seq and start the timeout clock.
  void begin(uint32_t seqNow, uint32_t nowMs) {
    _seqStart = seqNow; _startMs = nowMs; _state = State::Waiting;
  }

  // Ready as soon as the seq changes; TimedOut after timeoutMs with no change.
  // Ready/TimedOut are sticky until begin()/rearm()/reset().
  State poll(uint32_t seqNow, uint32_t nowMs) {
    if (_state != State::Waiting) return _state;
    if (seqNow != _seqStart) { _state = State::Ready; return _state; }
    if (nowMs - _startMs > timeoutMs) { _state = State::TimedOut; return _state; }
    return State::Waiting;
  }

  // The bump was for a different target: re-snapshot and keep waiting. The timeout
  // clock is NOT restarted, so a stream of other-contact replies still times out.
  void rearm(uint32_t seqNow) { _seqStart = seqNow; _state = State::Waiting; }

  void reset() { _state = State::Idle; }
  bool waiting() const { return _state == State::Waiting; }

private:
  uint32_t _seqStart = 0;
  uint32_t _startMs = 0;
  State    _state = State::Idle;
};

}  // namespace mishmesh
