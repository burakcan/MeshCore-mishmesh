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

// Pages overlap by a few px so a glyph the boundary cut in half turns up whole
// on the following one.
static const int PAGE_OVERLAP_PX = 8;

void Marquee::draw(Canvas& c, const mf_font_s* font, int x, int y, int availW, int rowH,
                   const char* text, DisplayDriver::Color col, uint32_t now) {
  _active = false;
  int ty = (rowH - c.fontHeight(font)) / 2; if (ty < 0) ty = 0;
  int tw = c.textWidth(font, text);
  if (tw <= availW) {
    c.drawTextEllipsized(font, x, y + ty, availW, text, col);
    return;
  }

  if (reducedMotion()) {
    // The re-entry guard has to allow for a flush that takes seconds, or every
    // frame would look like a fresh entry and the page would never advance.
    if (_lastDraw == 0 || now - _lastDraw > 6000 || _start == 0) _start = now;
    _lastDraw = now;
    const int step = availW > PAGE_OVERLAP_PX * 2 ? availW - PAGE_OVERLAP_PX : availW;
    int pages = 1;
    while ((pages - 1) * step + availW < tw) pages++;
    // One cycle is a short look at the start plus a full dwell on each of the
    // rest, repeating for as long as the row stays up.
    const uint32_t cycle = WRAP_MS + (uint32_t)(pages - 1) * PAGE_MS;
    const uint32_t t = (now - _start) % cycle;
    uint32_t idx = 0;
    if (t >= WRAP_MS) {
      idx = 1 + (t - WRAP_MS) / PAGE_MS;
      if (idx > (uint32_t)(pages - 1)) idx = pages - 1;
    }
    _active = true;
    Canvas clip = c.region(x, y, availW, rowH);
    clip.drawText(font, -(int)idx * step, ty, text, col);
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
