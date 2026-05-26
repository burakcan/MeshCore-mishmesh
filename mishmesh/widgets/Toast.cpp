#include <mishmesh/widgets/Toast.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

void Toast::show(const char* msg, uint32_t now, uint32_t durationMs) {
  strncpy(_msg, msg ? msg : "", sizeof(_msg) - 1);
  _msg[sizeof(_msg) - 1] = 0;
  _until = now + durationMs;
}

void Toast::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  view.fillRect(0, 0, w, h, DisplayDriver::DARK);   // clear behind the bar
  view.drawRect(0, 0, w, h, DisplayDriver::LIGHT);
  int ty = (h - view.fontHeight(fontBody())) / 2; if (ty < 0) ty = 0;
  view.drawTextEllipsized(fontBody(), w / 2, ty, w - 4, _msg, DisplayDriver::LIGHT, TextAlign::Center);
}

}  // namespace mishmesh
