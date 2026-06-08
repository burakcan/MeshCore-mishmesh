#pragma once

#include <stdint.h>
#include <helpers/ui/DisplayDriver.h>

struct mf_font_s;

namespace mishmesh {

enum class TextAlign { Left, Center, Right };

// A clipped drawing surface over a DisplayDriver: a value type carrying a
// drawing origin, a clip window, and the current frame time. The origin and the
// clip are tracked separately so a sub-region requested at a negative offset
// (e.g. a list row scrolled partly above the viewport) keeps its coordinate
// mapping while still being clipped to its parent's bounds - its overflow is
// discarded, not drawn outside the parent.
class Canvas {
  DisplayDriver* _d;
  int _ox, _oy;             // origin in device coordinates (local 0,0 maps here)
  int _w, _h;               // logical size, reported by width()/height()
  int _cl, _ct, _cr, _cb;   // clip window in local coords (left/top/right/bottom)
  uint32_t _now;            // frame time in ms, for animation

  Canvas(DisplayDriver* d, int ox, int oy, int w, int h,
         int cl, int ct, int cr, int cb, uint32_t now)
      : _d(d), _ox(ox), _oy(oy), _w(w), _h(h),
        _cl(cl), _ct(ct), _cr(cr), _cb(cb), _now(now) {}

public:
  explicit Canvas(DisplayDriver* d, uint32_t now = 0)
      : _d(d), _ox(0), _oy(0),
        _w(d ? d->width() : 0), _h(d ? d->height() : 0),
        _cl(0), _ct(0), _cr(d ? d->width() : 0), _cb(d ? d->height() : 0),
        _now(now) {}

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

  // Anchored text: x_right is the right edge, cx the horizontal centre. The
  // resolved origin is still origin-clipped like text().
  void textRight(int x_right, int y, const char* str, DisplayDriver::Color c);
  void textCentered(int cx, int y, const char* str, DisplayDriver::Color c);

  int textWidth(const char* str) const;

  // mcufont glyph text. (x,y) is the top-left of the target area; for Center/
  // Right alignment x is the centre / right edge. Returns the advance width.
  int textWidth(const mf_font_s* font, const char* str) const;
  int lineHeight(const mf_font_s* font) const;   // line advance
  int fontHeight(const mf_font_s* font) const;   // bounding-box height, for centering
  void drawText(const mf_font_s* font, int x, int y, const char* str,
                DisplayDriver::Color c, TextAlign align = TextAlign::Left);
  // Word-wraps within w; returns the y just below the last line.
  int drawTextWrapped(const mf_font_s* font, int x, int y, int w,
                      const char* str, DisplayDriver::Color c);
  // Renders a single glyph (used for icon fonts) at (x,y).
  void drawGlyph(const mf_font_s* font, int x, int y, uint16_t codepoint,
                 DisplayDriver::Color c);
  // Single-line text truncated with "..." when it would exceed maxWidth.
  void drawTextEllipsized(const mf_font_s* font, int x, int y, int maxWidth,
                          const char* str, DisplayDriver::Color c,
                          TextAlign align = TextAlign::Left);
  // 50% checkerboard fill (modal scrim / dithered shadow). Per-pixel on mono;
  // use for small regions or a scrim, not large solid fills.
  void fillStipple(int x, int y, int w, int h, DisplayDriver::Color c);

  // Blit a full-screen column-major 1bpp buffer to the panel (device coords). See
  // DisplayDriver::blitColumnMajor1bpp. Intended for full-frame sources (e.g. a game).
  void blit1bpp(const uint8_t* buf, int w, int h);
};

}  // namespace mishmesh
