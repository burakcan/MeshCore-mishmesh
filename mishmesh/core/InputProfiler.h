#pragma once

#include <stdint.h>

namespace mishmesh {

// Lightweight, allocation-free UI-responsiveness profiler. Pure accounting so it
// host-tests without a display. The host feeds it: one tick() per loop pass (loop
// period / starvation), recordRender() per frame, and the input counters as events
// flow through pumpInput/dispatch. Values are since-boot; the on-screen overlay
// (enabled with -D MISHMESH_INPUT_PROFILE) reads the public fields directly.
//
// Triage for a "the button didn't register" report - compare against your own
// physical press count:
//   polled     - events the sources emitted (a press edge was actually sampled)
//   dispatched - of those, ones acted on; (polled - dispatched) is what software
//                bounce/debounce coalescing dropped
//   blindGaps  - loop passes whose gap was wide enough to swallow a whole tap; a
//                press landing entirely inside one is never sampled at all
// fingers > polled     => starvation or hardware: the edge was missed (blindGaps
//                         should be climbing in step)
// polled  > dispatched => software gating ate an edge that was sampled fine
struct InputProfiler {
  // A loop gap at least this wide can miss a short tap that goes down-and-up
  // entirely between two polls. ~50ms is a brisk-but-real tap length.
  static const uint32_t BLIND_GAP_MS = 50;

  uint32_t maxStall  = 0;   // worst loop gap seen (ms)
  uint32_t maxRender = 0;   // worst frame compose+flush (ms)
  uint32_t lastRender = 0;
  uint32_t blindGaps  = 0;  // loop passes with gap >= BLIND_GAP_MS
  uint32_t polled     = 0;  // discrete (non-repeat) events emitted by sources
  uint32_t dispatched = 0;  // of those, delivered to an applet
  uint32_t renders    = 0;

  void reset() { *this = InputProfiler(); }

  // One call per loop pass, with the loop's millis(). The first call only seeds
  // the clock so no phantom gap is charged from before profiling started.
  void tick(uint32_t now_ms) {
    if (_seeded) {
      uint32_t dt = now_ms - _last_tick;
      _loops++;
      _dtSum += dt;
      if (dt > maxStall) maxStall = dt;
      if (dt >= BLIND_GAP_MS) blindGaps++;
    }
    _last_tick = now_ms;
    _seeded = true;
  }

  void recordRender(uint32_t ms) {
    if (ms > maxRender) maxRender = ms;
    lastRender = ms;
    renders++;
  }
  void recordPolled()     { polled++; }
  void recordDispatched() { dispatched++; }

  uint32_t avgLoopMs() const { return _loops ? (uint32_t)(_dtSum / _loops) : 0; }

private:
  bool     _seeded = false;
  uint32_t _last_tick = 0;
  uint32_t _loops = 0;
  uint64_t _dtSum = 0;
};

}  // namespace mishmesh
