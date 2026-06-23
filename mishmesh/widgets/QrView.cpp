// mishmesh/widgets/QrView.cpp
#include <mishmesh/widgets/QrView.h>

extern "C" {
#include <qrcode.h>   // vendored ricmoo QR generator (mishmesh/vendor/qrcode)
}

namespace mishmesh {

bool QrView::build(const char* text) {
  _size = 0;
  if (!text || !text[0]) return false;
  QRCode qr;
  // Probe from the smallest version up; initText returns 0 on success and -1
  // when the data doesn't fit that version (it guards its own buffers, so this
  // is safe - the first fit is also the lowest-density code).
  for (uint8_t v = 1; v <= MAX_VERSION; ++v) {
    if (qrcode_initText(&qr, _buf, v, ECC_LOW, text) == 0) {
      _size = qr.size;
      return true;
    }
  }
  return false;
}

void QrView::draw(Canvas& c, int x, int y, int w, int h) const {
  if (_size <= 0) return;
  const int quiet = 2;
  const int total = _size + 2 * quiet;   // modules including the quiet border
  int side = (w < h) ? w : h;            // fill the limiting dimension (a square)
  if (side < total) return;              // can't give even 1px/module - skip

  // Theme-neutral: pre-apply themedColor so fillRect's own themedColor cancels
  // (it's an involution), leaving the panel's true light bg / dark modules
  // regardless of the UI theme - an inverted QR won't reliably scan.
  const DisplayDriver::Color bg = themedColor(DisplayDriver::LIGHT);
  const DisplayDriver::Color fg = themedColor(DisplayDriver::DARK);

  c.fillRect(x, y, side, side, bg);      // white square incl. the quiet border

  // Map module edges onto pixels so the code fills `side` exactly. On tiny
  // panels side/total isn't integral, so modules end up 1-2px - but edges stay
  // consistent (no gaps/overlap), which decoders sample fine, and a full-height
  // code resolves far better than a 1px-per-module integer-scaled one.
  // getModule only reads size + modules; rewrap our buffer rather than keep a
  // QRCode member (which would drag qrcode.h into the header).
  QRCode qr;
  qr.size = (uint8_t)_size;
  qr.modules = const_cast<uint8_t*>(_buf);
  for (int my = 0; my < _size; ++my) {
    const int y0 = y + (my + quiet) * side / total;
    const int y1 = y + (my + quiet + 1) * side / total;
    for (int mx = 0; mx < _size; ++mx) {
      if (!qrcode_getModule(&qr, (uint_fast8_t)mx, (uint_fast8_t)my)) continue;
      const int x0 = x + (mx + quiet) * side / total;
      const int x1 = x + (mx + quiet + 1) * side / total;
      c.fillRect(x0, y0, x1 - x0, y1 - y0, fg);
    }
  }
}

}  // namespace mishmesh
