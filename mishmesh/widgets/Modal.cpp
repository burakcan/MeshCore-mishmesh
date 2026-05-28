#include <mishmesh/widgets/Modal.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

Canvas drawModalChrome(Canvas& c) {
  int w = c.width(), h = c.height();
  c.fillStipple(0, 0, w, h, DisplayDriver::DARK);   // scrim dims content behind

  int bw = w - 16, bh = h - 16;
  int bx = (w - bw) / 2, by = (h - bh) / 2;
  c.fillStipple(bx + 2, by + 2, bw, bh, DisplayDriver::DARK);   // dithered drop shadow

  Canvas box = c.region(bx, by, bw, bh);
  box.fillRect(0, 0, bw, bh, DisplayDriver::DARK);
  box.drawRect(0, 0, bw, bh, DisplayDriver::LIGHT);
  return box;
}

}  // namespace mishmesh
