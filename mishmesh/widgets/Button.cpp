#include <mishmesh/widgets/Button.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

static const int ICON_PX = 12;   // iconFont() glyph box
static const int PAD      = 3;   // min inset from the frame
static const int GAP      = 3;   // icon-to-label gap

void Button::draw(Canvas& c, int x, int y, int w, int h) {
  DisplayDriver::Color fg = _focused ? DisplayDriver::DARK : DisplayDriver::LIGHT;
  if (_focused) c.fillRect(x, y, w, h, DisplayDriver::LIGHT);   // solid = pressed/selected
  else          c.drawRect(x, y, w, h, DisplayDriver::LIGHT);   // outline = idle

  const mf_font_s* f = fontBody();
  int tw  = (_label && _label[0]) ? c.textWidth(f, _label) : 0;
  int iw  = _icon ? ICON_PX : 0;
  int gap = (iw && tw) ? GAP : 0;
  int avail = w - 2 * PAD - iw - gap;
  if (tw > avail) tw = avail;

  // Center the icon+label group; clamp to the left pad if it would overflow.
  int cx = x + (w - (iw + gap + tw)) / 2;
  if (cx < x + PAD) cx = x + PAD;

  if (_icon) {
    c.drawGlyph(iconFont(), cx, y + (h - ICON_PX) / 2, _icon, fg);
    cx += iw + gap;
  }
  if (tw > 0) {
    c.drawTextEllipsized(f, cx, y + (h - c.fontHeight(f)) / 2, tw, _label, fg);
  }
}

}  // namespace mishmesh
