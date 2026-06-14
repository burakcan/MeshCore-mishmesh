#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
// [mishmesh]
#include <mishmesh/core/MessagesService.h>
// [/mishmesh]

namespace mishmesh {

void HomeApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app = ctx.app;
  // [mishmesh]
  _msgs = ctx.messages;
  // [/mishmesh]
}

int HomeApplet::onRender(Canvas& c) {
  int w = c.width();
  int h = c.height();

  if (_app) {
    _bar.setTitle(_app->nodeName());
    _bar.setBattery(_app->batteryMillivolts());
  }
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.draw(c, 0, 0, w, barH);

  uint32_t t = _app ? _app->epochSeconds() : 0;
  char clock[6];
  snprintf(clock, sizeof(clock), "%02u:%02u",
           (unsigned)((t / 3600) % 24), (unsigned)((t / 60) % 60));
  int ch = c.fontHeight(fontNum());
  int hintH = _menu ? c.fontHeight(fontBody()) + 2 : 0;
  c.drawText(fontNum(), w / 2, barH + (h - barH - hintH - ch) / 2, clock,
             DisplayDriver::LIGHT, TextAlign::Center);

  if (_menu)
    c.drawText(fontBody(), w / 2, h - hintH, "Select for apps",
               DisplayDriver::LIGHT, TextAlign::Center);

  // [mishmesh] total-unread indicator: mail glyph + count in bottom-left corner
  if (_msgs && _msgs->totalNotifyUnread()) {
    char b[8]; snprintf(b, sizeof(b), "%u", _msgs->totalNotifyUnread());
    c.drawGlyph(iconFont(), 2, h - 14, (uint16_t)Icon::Mail, DisplayDriver::LIGHT);
    c.drawText(fontBody(), 16, h - 14, b, DisplayDriver::LIGHT);
  }
  // [/mishmesh]
  return 1000;
}

bool HomeApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select && _menu && _host) {
    _host->push(_menu);
    return true;
  }
  return false;
}

}  // namespace mishmesh
