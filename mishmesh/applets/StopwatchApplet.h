#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

// Demo leaf applet. Select toggles run/stop, NavDown resets, Back exits. Time
// runs off the Canvas frame clock, so it needs no app services.
class StopwatchApplet : public Applet {
  bool _running;
  uint32_t _accum;   // ms banked while stopped
  uint32_t _start;   // frame tick at last start
  uint32_t _now;     // last frame tick, for input-time toggles
public:
  StopwatchApplet()
      : Applet("Stopwatch"), _running(false), _accum(0), _start(0), _now(0) {}

  void reset() { _running = false; _accum = 0; _start = 0; }
  void toggle(uint32_t now);
  uint32_t elapsedMs(uint32_t now) const;

  void onStart(AppletContext&) override { reset(); }
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
