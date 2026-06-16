#include <mishmesh/applets/SettingsDetailApplet.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

void SettingsDetailApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  if (_panel) {
    _panel->begin(ctx);
    _bar.setTitle(_panel->title());
  }
}

void SettingsDetailApplet::onForeground() {
  if (_panel) _panel->onShow();
}

int SettingsDetailApplet::onRender(Canvas& c) {
  int bw = 0, bh = 0; _bar.measure(bw, bh);
  _bar.setBattery(_app ? _app->batteryMillivolts() : 0);
  _bar.draw(c, 0, 0, c.width(), bh);
  if (!_panel) return 1000;
  return _panel->renderBody(c, 0, bh + 1, c.width(), c.height() - (bh + 1));
}

bool SettingsDetailApplet::onInput(InputEvent ev) {
  return _panel && _panel->onInput(ev);   // unconsumed (incl. Back) bubbles -> host pops
}

SettingsDetailApplet& settingsDetailApplet() {
  static SettingsDetailApplet s;
  return s;
}

}  // namespace mishmesh
