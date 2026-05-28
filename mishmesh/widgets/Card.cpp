#include <mishmesh/widgets/Card.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

int Card::height(Canvas& c) const {
  return c.fontHeight(fontSubtitle()) + 3 + c.fontHeight(fontBody()) + 4;
}

void Card::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas v = c.region(x, y, w, h);
  int barH = v.fontHeight(fontSubtitle()) + 3;
  v.fillRect(0, 0, w, barH, DisplayDriver::LIGHT);                 // inverted title bar

  int titleW = w - 6;
  if (_favourite) {                                               // star at the bar's right edge
    int ih = v.fontHeight(iconFont());
    int iy = (barH - ih) / 2; if (iy < 0) iy = 0;
    v.drawGlyph(iconFont(), w - ih - 2, iy, (uint16_t)Icon::Star, DisplayDriver::DARK);
    titleW -= ih + 2;
  }
  _titleMarquee.draw(v, fontSubtitle(), 3, 0, titleW, barH, _title, DisplayDriver::DARK, v.now());
  v.drawTextEllipsized(fontBody(), 3, barH + 2, w - 6, _info, DisplayDriver::LIGHT);
  v.drawRect(0, 0, w, h, DisplayDriver::LIGHT);                    // outer border
}

}  // namespace mishmesh
