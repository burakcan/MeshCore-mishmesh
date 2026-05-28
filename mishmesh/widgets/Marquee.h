#pragma once

#include <stdint.h>
#include <helpers/ui/DisplayDriver.h>

struct mf_font_s;

namespace mishmesh {

class Canvas;

// Scrolling-text helper. draw() renders `text` ellipsized when it fits, or
// marquees it (hold, scroll, hold, loop) when it overflows availW, phased from
// when it became active. reset() restarts the phase (call on content change);
// the phase also restarts after an off-screen render gap. active() reports
// whether it is currently marqueeing, so owners can request faster re-renders.
class Marquee {
  uint32_t _start;
  uint32_t _lastDraw;
  bool     _active;
public:
  Marquee() : _start(0), _lastDraw(0), _active(false) {}
  void reset() { _start = 0; }
  bool active() const { return _active; }
  void draw(Canvas& c, const mf_font_s* font, int x, int y, int availW, int rowH,
            const char* text, DisplayDriver::Color col, uint32_t now);
};

}  // namespace mishmesh
