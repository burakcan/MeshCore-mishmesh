#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

bool TabBar::onInput(InputEvent ev) {
  if (ev == InputEvent::NavRight) { int prev = _sel; if (_sel < _count - 1) _sel++; return _sel != prev; }
  if (ev == InputEvent::NavLeft)  { int prev = _sel; if (_sel > 0) _sel--;          return _sel != prev; }
  return false;
}

void TabBar::measure(int& w, int& h) const {
  w = 0;
  h = 13;
}

void TabBar::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  if (_count == 0) return;
  int cell = w / _count;
  for (int i = 0; i < _count; i++) {
    int cx = i * cell;
    bool sel = (i == _sel);
    if (sel) view.fillRect(cx, 0, cell, h, DisplayDriver::LIGHT);
    DisplayDriver::Color col = sel ? DisplayDriver::DARK : DisplayDriver::LIGHT;
    int ty = (h - view.fontHeight(fontBody())) / 2; if (ty < 0) ty = 0;
    if (_icons[i]) {
      int ih = view.fontHeight(iconFont());
      view.drawGlyph(iconFont(), cx + (cell - ih) / 2, (h - ih) / 2, _icons[i], col);
    } else {
      Canvas cellv = view.region(cx, 0, cell, h);
      cellv.drawTextEllipsized(fontBody(), cell / 2, ty, cell - 2, _labels[i], col, TextAlign::Center);
    }
  }
  view.fillRect(0, h - 1, w, 1, DisplayDriver::LIGHT);
}

}  // namespace mishmesh
