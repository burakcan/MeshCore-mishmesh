#include <mishmesh/widgets/Marquee.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

// Offset (px) given ms since the row became active: hold (start delay), scroll
// to reveal the tail, hold, then repeat.
static int marqueeOffset(uint32_t elapsed, int overflow) {
  const uint32_t PAUSE = 1100;
  const uint32_t PER_PX = 28;
  uint32_t scrollMs = (uint32_t)overflow * PER_PX;
  uint32_t cycle = PAUSE + scrollMs + PAUSE;
  uint32_t t = elapsed % cycle;
  if (t < PAUSE) return 0;
  if (t < PAUSE + scrollMs) return (int)((t - PAUSE) / PER_PX);
  return overflow;
}

void Marquee::draw(Canvas& c, const mf_font_s* font, int x, int y, int availW, int rowH,
                   const char* text, DisplayDriver::Color col, uint32_t now) {
  _active = false;
  int ty = (rowH - c.fontHeight(font)) / 2; if (ty < 0) ty = 0;
  int tw = c.textWidth(font, text);
  if (tw <= availW || reducedMotion()) {
    c.drawTextEllipsized(font, x, y + ty, availW, text, col);
    return;
  }
  bool reentered = (_lastDraw == 0) || (now - _lastDraw > 300);
  if (reentered || _start == 0) _start = now;
  _lastDraw = now;
  _active = true;

  int overflow = tw - availW + 6;
  int off = marqueeOffset(now - _start, overflow);
  Canvas clip = c.region(x, y, availW, rowH);
  clip.drawText(font, -off, ty, text, col);
}

}  // namespace mishmesh
