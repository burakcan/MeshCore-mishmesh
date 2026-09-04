// mishmesh/applets/AboutApplet.cpp
#include "AboutApplet.h"
#include <mishmesh/applets/onboarding_logo.h>   // MISHMESH_LOGO wordmark
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>   // snprintf

namespace mishmesh {

// Full URL (with scheme) so a phone camera opens it directly. 27 bytes => a
// small QR (~v2, 25 modules) that stays crisp at 2px/module on a 128x64 panel.
static const char SUPPORT_URL[] = "https://ko-fi.com/burak_can";

const char* AboutApplet::qrTextForTest() const { return SUPPORT_URL; }

void AboutApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _version[0] = 0;
  _mmVersion[0] = 0;
  if (_app) {
    SystemStats s;
    if (_app->systemStats(s)) {
      if (s.meshcoreVersion) snprintf(_version, sizeof(_version), "mc %s", s.meshcoreVersion);
      if (s.mishmeshVersion) snprintf(_mmVersion, sizeof(_mmVersion), "mm %s", s.mishmeshVersion);
    }
  }
  _qr.build(SUPPORT_URL);
}

int AboutApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();

  // The QR takes a square off the short edge and the wordmark + support text fill
  // what is left. On a landscape canvas that leftover is a column beside it; on a
  // portrait one it is a band underneath, and splitting sideways there left the
  // text with no width at all.
  const bool stacked = h > w;
  const int side = stacked ? w : h;
  if (_qr.valid()) _qr.draw(c, 0, 0, side, side);
  else c.drawTextCentered(fontBody(), 0, 0, side, side, "QR too big", DisplayDriver::LIGHT);

  const int rx = stacked ? 0 : side + 4;
  const int ry = stacked ? side + 4 : 0;
  if (rx < w && ry < h) {
    Canvas r = c.region(rx, ry, w - rx, h - ry);
    const int rw = r.width();
    int lx = (rw > MISHMESH_LOGO_W) ? (rw - MISHMESH_LOGO_W) / 2 : 0;
    r.drawXbm(lx, 2, MISHMESH_LOGO, MISHMESH_LOGO_W, MISHMESH_LOGO_H, DisplayDriver::LIGHT);

    // Caption is the recessive tier, which on a 64px panel is the only thing that
    // fits. A taller one has room for body type, and this is a screen people read
    // rather than glance at.
    const Font* f = (r.height() >= 100 || rw >= 100) ? fontBody() : fontCaption();
    const int lh = r.fontHeight(f) + 1;

    int y = 2 + MISHMESH_LOGO_H + 3;
    y = r.drawTextWrapped(f, 0, y, rw, "Support development", DisplayDriver::LIGHT) + 2;
    r.drawText(f, 0, y, "ko-fi.com/", DisplayDriver::LIGHT); y += lh;
    r.drawText(f, 0, y, "burak_can", DisplayDriver::LIGHT);

    // Both versions pinned to the bottom: mishmesh on the last row, firmware
    // just above it.
    int vy = r.height() - lh;
    if (_mmVersion[0]) { r.drawText(f, 0, vy, _mmVersion, DisplayDriver::LIGHT); vy -= lh; }
    if (_version[0])     r.drawText(f, 0, vy, _version, DisplayDriver::LIGHT);
  }
  return 1000;
}

bool AboutApplet::onInput(InputEvent) {
  return false;   // read-only; Back bubbles -> host pops
}

AboutApplet& aboutApplet() {
  static AboutApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&aboutApplet(), Placement::AppMenu, "About", 10,
                              (uint16_t)Icon::Coffee);   // coffee cup = support

}  // namespace mishmesh
