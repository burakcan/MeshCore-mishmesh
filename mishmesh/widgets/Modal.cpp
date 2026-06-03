#include <mishmesh/widgets/Modal.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

// A solid bordered box, nothing else. The scrim and drop shadow were both removed
// (their full-screen / per-frame dither was too costly on mono OLED and made the
// in-context menu animations lag); all modals and dialogs share this plain look.
Canvas drawModalChrome(Canvas& c) {
  int w = c.width(), h = c.height();
  int bw = w - 16, bh = h - 16;
  int bx = (w - bw) / 2, by = (h - bh) / 2;
  Canvas box = c.region(bx, by, bw, bh);
  box.fillRect(0, 0, bw, bh, DisplayDriver::DARK);
  box.drawRect(0, 0, bw, bh, DisplayDriver::LIGHT);
  return box;
}

}  // namespace mishmesh
