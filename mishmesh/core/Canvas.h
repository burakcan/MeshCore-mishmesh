#pragma once

#include <stdint.h>
#include <helpers/ui/DisplayDriver.h>

namespace mishmesh {

// A clipped drawing surface over a DisplayDriver: a value type carrying an
// origin offset, clip bounds, and the current frame time.
class Canvas {
  DisplayDriver* _d;
  int _ox, _oy;     // origin in device coordinates
  int _w, _h;       // clip bounds
  uint32_t _now;    // frame time in ms, for animation

  Canvas(DisplayDriver* d, int ox, int oy, int w, int h, uint32_t now)
      : _d(d), _ox(ox), _oy(oy), _w(w), _h(h), _now(now) {}

public:
  explicit Canvas(DisplayDriver* d, uint32_t now = 0)
      : _d(d), _ox(0), _oy(0),
        _w(d ? d->width() : 0), _h(d ? d->height() : 0), _now(now) {}

  int width() const { return _w; }
  int height() const { return _h; }
  uint32_t now() const { return _now; }
  void setNow(uint32_t now) { _now = now; }
  DisplayDriver* display() const { return _d; }

  // A sub-surface in local coordinates, clamped to this canvas's bounds.
  Canvas region(int x, int y, int w, int h) const;

  void fillRect(int x, int y, int w, int h, DisplayDriver::Color c);
  void drawRect(int x, int y, int w, int h, DisplayDriver::Color c);

  // DisplayDriver can't clip glyphs: text is dropped only when its origin is
  // outside the canvas, so it may still overflow the right edge.
  void text(int x, int y, const char* str, DisplayDriver::Color c);
};

}  // namespace mishmesh
