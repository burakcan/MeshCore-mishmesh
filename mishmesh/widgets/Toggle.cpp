#include <mishmesh/widgets/Toggle.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void Toggle::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  DisplayDriver::Color bg = (_fg == DisplayDriver::DARK) ? DisplayDriver::LIGHT : DisplayDriver::DARK;
  const char* s = stateText(_on);
  int ty = (h - view.fontHeight(fontBody())) / 2;
  if (_on) {
    view.fillRect(0, 0, w, h, _fg);
    view.drawTextEllipsized(fontBody(), w / 2, ty, w, s, bg, TextAlign::Center);
  } else {
    view.drawRect(0, 0, w, h, _fg);
    view.drawTextEllipsized(fontBody(), w / 2, ty, w, s, _fg, TextAlign::Center);
  }
}

}  // namespace mishmesh
