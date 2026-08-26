#include <mishmesh/widgets/Button.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

static const int ICON_PX = 12;   // iconFont() glyph box
static const int PAD      = 3;   // min inset from the frame
static const int GAP      = 3;   // icon-to-label gap

int Button::preferredWidth(Canvas& c) const {
  int tw  = (_label && _label[0]) ? c.textWidth(fontBody(), _label) : 0;
  int iw  = _icon ? ICON_PX : 0;
  int gap = (iw && tw) ? GAP : 0;
  return iw + gap + tw + PAD * 2;
}

void Button::draw(Canvas& c, int x, int y, int w, int h) {
  DisplayDriver::Color fg = _focused ? DisplayDriver::DARK : DisplayDriver::LIGHT;
  if (_focused) c.fillRect(x, y, w, h, DisplayDriver::LIGHT);   // solid = pressed/selected
  else          c.drawRect(x, y, w, h, DisplayDriver::LIGHT);   // outline = idle

  const mf_font_s* f = fontBody();
  int iw  = _icon ? ICON_PX : 0;
  int inner = w - PAD * 2 - iw;
  if (inner < 0) inner = 0;
  int tw = (_label && _label[0]) ? c.textWidth(f, _label) : 0;
  int gap = (iw && tw) ? GAP : 0;
  // The caller may hand us a box narrower than the content (a long label on a
  // narrow screen); truncate rather than spilling text past the frame.
  int drawW = tw;
  if (drawW > inner - gap) { drawW = inner - gap; if (drawW < 0) drawW = 0; }

  int cx = x + (w - (iw + gap + drawW)) / 2;
  if (cx < x + PAD) cx = x + PAD;

  if (_icon) {
    c.drawGlyph(iconFont(), cx, y + (h - ICON_PX) / 2, _icon, fg);
    cx += iw + gap;
  }
  if (drawW) {
    c.drawTextEllipsized(f, cx, y + (h - c.fontHeight(f)) / 2, drawW, _label, fg);
  }
}

}  // namespace mishmesh
