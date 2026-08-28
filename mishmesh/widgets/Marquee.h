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
//
// With motion reduced there is no scroll - a per-frame slide on a panel that
// takes hundreds of ms to flush is just a smear - but the tail still has to be
// readable, so the window steps along in whole-page jumps instead and keeps
// looping, holding the first page for less than the rest so the wrap does not
// read as a stall.
class Marquee {
  uint32_t _start;
  uint32_t _lastDraw;
  bool     _active;
public:
  // Reduced-motion dwell per window. Renders arrive about a second apart on a
  // slow panel, so much below this the steps land at visibly uneven intervals.
  static const uint32_t PAGE_MS = 1800;
  static const uint32_t WRAP_MS = 1100;   // the first page, which you have read already

  Marquee() : _start(0), _lastDraw(0), _active(false) {}
  void reset() { _start = 0; _active = false; }   // not marqueeing until a draw proves otherwise
  bool active() const { return _active; }
  void draw(Canvas& c, const mf_font_s* font, int x, int y, int availW, int rowH,
            const char* text, DisplayDriver::Color col, uint32_t now);
};

}  // namespace mishmesh
