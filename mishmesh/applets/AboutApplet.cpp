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

  // QR fills a full-height square anchored top-left; the wordmark + support text
  // ride in the leftover right column (same split as ChannelShareApplet).
  const int side = (h <= w) ? h : w;
  if (_qr.valid()) _qr.draw(c, 0, 0, side, side);
  else c.drawTextCentered(fontBody(), 0, 0, side, h, "QR too big", DisplayDriver::LIGHT);

  const int rx = side + 4;
  if (rx < w) {
    Canvas r = c.region(rx, 0, w - rx, h);
    const int rw = r.width();
    int lx = (rw > MISHMESH_LOGO_W) ? (rw - MISHMESH_LOGO_W) / 2 : 0;
    r.drawXbm(lx, 2, MISHMESH_LOGO, MISHMESH_LOGO_W, MISHMESH_LOGO_H);

    int y = 2 + MISHMESH_LOGO_H + 3;
    y = r.drawTextWrapped(fontCaption(), 0, y, rw, "Support development",
                          DisplayDriver::LIGHT) + 2;
    r.drawText(fontCaption(), 0, y, "ko-fi.com/", DisplayDriver::LIGHT); y += 6;
    r.drawText(fontCaption(), 0, y, "burak_can", DisplayDriver::LIGHT);

    // Both versions pinned to the bottom, recessive: mishmesh on the last row,
    // firmware just above it.
    int vy = h - 6;
    if (_mmVersion[0]) { r.drawText(fontCaption(), 0, vy, _mmVersion, DisplayDriver::LIGHT); vy -= 6; }
    if (_version[0])     r.drawText(fontCaption(), 0, vy, _version, DisplayDriver::LIGHT);
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
