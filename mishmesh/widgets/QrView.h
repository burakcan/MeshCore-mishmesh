// mishmesh/widgets/QrView.h
#pragma once
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

// Renders a QR code from a text string onto a Canvas. Encoding happens in
// build() (call from onStart()/begin() - a fixed module buffer lives in the
// object, so no heap). draw() paints it theme-neutral (always a light
// background with dark modules) so it stays scannable in either light or dark
// mode. Backed by the vendored ricmoo QR generator (mishmesh/vendor/qrcode);
// the QRCode type is kept out of this header so its generic MODE_*/ECC_* macros
// don't leak into every includer.
class QrView {
public:
  // Encode `text` at the smallest version that fits, ECC level L (minimises the
  // module count, keeping modules large on small panels). Returns false if it
  // does not fit MAX_VERSION; draw() then no-ops. Re-callable.
  bool build(const char* text);
  bool valid() const { return _size > 0; }
  int  moduleCount() const { return _size; }   // modules per side, 0 if unbuilt

  // Draw at the top-left of (x,y,w,h), filling a square of side min(w,h) with a
  // 2-module quiet border. Module edges are mapped onto pixels so the code fills
  // the box exactly (modules may be 1-2px on small panels). No-op until build()
  // succeeds, or if the box is too small for even 1px per module.
  void draw(Canvas& c, int x, int y, int w, int h) const;

private:
  // v10 => 57 modules/side => 407 module bytes. Ample for the channel-share URI
  // (~70-160 bytes at ECC-L, which lands around v4-v8).
  static const int MAX_VERSION = 10;
  static const int MAX_MODULES = 4 * MAX_VERSION + 17;
  static const int BUF_BYTES   = (MAX_MODULES * MAX_MODULES + 7) / 8;
  uint8_t _buf[BUF_BYTES];
  int     _size = 0;   // modules per side
};

}  // namespace mishmesh
