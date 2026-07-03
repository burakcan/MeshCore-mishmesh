// mishmesh/applets/RepeaterManageApplet.cpp
#include <mishmesh/applets/RepeaterManageApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

void RepeaterManageApplet::setTarget(const uint8_t* pubKey, const char* name) {
  memcpy(_pub, pubKey, 6);
  if (name) { strncpy(_name, name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = 0; }
  else _name[0] = 0;
}

int RepeaterManageApplet::onRender(Canvas& c) {
  const int w = c.width();
  const Font* body = fontBody();
  const Font* cap = fontCaption();
  int lh = c.lineHeight(body);
  c.drawText(body, w / 2, 6, _name[0] ? _name : "Repeater",
             DisplayDriver::LIGHT, TextAlign::Center);
  c.drawText(cap, w / 2, 6 + lh + 4, "Logged in",
             DisplayDriver::LIGHT, TextAlign::Center);
  return 1000;   // static screen: idle repaint cadence (host also re-renders on input/invalidation)
}

RepeaterManageApplet& repeaterManageApplet() {
  static RepeaterManageApplet s_repeaterManage;
  return s_repeaterManage;
}

}  // namespace mishmesh
