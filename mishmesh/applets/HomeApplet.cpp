#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>

namespace mishmesh {

void HomeApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app = ctx.app;
}

int HomeApplet::onRender(Canvas& c) {
  int w = c.width();
  int h = c.height();

  if (_app) {
    _bar.setTitle(_app->nodeName());
    _bar.setBattery(_app->batteryMillivolts());
  }
  _bar.draw(c, 0, 0, w, 12);

  uint32_t t = _app ? _app->epochSeconds() : 0;
  char clock[6];
  snprintf(clock, sizeof(clock), "%02u:%02u",
           (unsigned)((t / 3600) % 24), (unsigned)((t / 60) % 60));
  c.textCentered(w / 2, h / 2 - 4, clock, DisplayDriver::LIGHT);

  if (_menu) c.textCentered(w / 2, h - 10, "Select: apps", DisplayDriver::LIGHT);
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
