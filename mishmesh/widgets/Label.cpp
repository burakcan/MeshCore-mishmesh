#include <mishmesh/widgets/Label.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

Label::Label()
    : _text(""), _font(nullptr), _color(DisplayDriver::LIGHT),
      _align(TextAlign::Left), _icon(0) {}

void Label::set(const char* text, const mf_font_s* font,
                DisplayDriver::Color color, TextAlign align) {
  _text = text ? text : "";
  _font = font;
  _color = color;
  _align = align;
}

void Label::draw(Canvas& c, int x, int y, int w, int h) {
  if (!_font) return;
  Canvas view = c.region(x, y, w, h);
  int ty = (h - view.fontHeight(_font)) / 2;
  if (ty < 0) ty = 0;
  int tx = 0;
  int avail = w;
  if (_icon) {
    int ih = view.fontHeight(iconFont());
    int iy = (h - ih) / 2; if (iy < 0) iy = 0;
    view.drawGlyph(iconFont(), 0, iy, _icon, _color);
    int adv = view.textWidth(iconFont(), " ") + ih;  // glyph + gap
    tx = adv; avail = w - adv;
  }
  // Left align is the common case; right/center anchor differently.
  if (_align == TextAlign::Left) {
    view.drawTextEllipsized(_font, tx, ty, avail, _text, _color, TextAlign::Left);
  } else if (_align == TextAlign::Right) {
    view.drawTextEllipsized(_font, w - 1, ty, avail, _text, _color, TextAlign::Right);
  } else {
    view.drawTextEllipsized(_font, tx + avail / 2, ty, avail, _text, _color, TextAlign::Center);
  }
}

}  // namespace mishmesh
