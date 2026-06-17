#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/TimeFormat.h>
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
  int16_t off = _app ? _app->tzOffsetMinutes() : 0;
  bool fmt12h = _app ? _app->timeFormat12h() : false;
  LocalTime lt = applyTz(t, off);
  char clock[12];
  formatClock(clock, sizeof(clock), lt, fmt12h);

  int ch = c.fontHeight(fontNum());
  int dateH = t ? c.fontHeight(fontBody()) + 2 : 0;
  int hintH = _menu ? c.fontHeight(fontBody()) + 2 : 0;
  int blockTop = barH + (h - barH - hintH - ch - dateH) / 2;
  c.drawText(fontNum(), w / 2, blockTop, clock,
             DisplayDriver::LIGHT, TextAlign::Center);
  if (t) {
    char date[24];
    DateFormat df = _app ? (DateFormat)_app->dateFormat() : DateFormat::DMY;
    formatShortDate(date, sizeof(date), lt, df);
    c.drawText(fontBody(), w / 2, blockTop + ch, date,
               DisplayDriver::LIGHT, TextAlign::Center);
  }

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
