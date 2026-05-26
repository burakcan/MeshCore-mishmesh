#pragma once

#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

// Transient feedback message ("Path reset", "Cleared", ...) shown for a short
// time over existing content. Timed off the Canvas frame clock: the owner calls
// show() with the current now() (typically from onRender after latching a
// pending message), draws it while active(), and keeps re-rendering until it
// expires. Drawn as a small bordered bar; place it where the owner wants.
class Toast : public Widget {
  char     _msg[28];
  uint32_t _until;   // frame-clock ms until which the toast is visible
public:
  Toast() : _until(0) { _msg[0] = 0; }
  void show(const char* msg, uint32_t now, uint32_t durationMs = 1400);
  bool active(uint32_t now) const { return _until != 0 && now < _until; }
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
