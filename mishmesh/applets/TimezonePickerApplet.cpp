#include <mishmesh/applets/TimezonePickerApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/WorldClock.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

TimezonePickerApplet::TimezonePickerApplet() : Applet("Time zone") {}

int TimezonePickerApplet::count() const { return worldCityCount(); }

const char* TimezonePickerApplet::label(int i) const {
  if (i < 0 || i >= worldCityCount()) return "";
  return worldCity(i).name;
}

const char* TimezonePickerApplet::value(int i) const {
  if (i < 0 || i >= worldCityCount()) return "";
  uint32_t now = _app ? _app->epochSeconds() : 0;
  formatOffset(_val, sizeof(_val), worldCityOffsetNow(i, now));
  return _val;
}

bool TimezonePickerApplet::radioOn(int i) const { return i == _pending; }

void TimezonePickerApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
  if (_pending >= 0 && _pending < worldCityCount()) _list.setSelected(_pending);
}

int TimezonePickerApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle("Time zone");
  _bar.draw(c, 0, 0, w, barH);
  _list.draw(c, 0, barH, w, h - barH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool TimezonePickerApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    if (_cb) _cb(_ctx, _list.selected());
    if (_host) _host->pop();
    return true;
  }
  return false;   // Back bubbles -> host pops
}

TimezonePickerApplet& timezonePickerApplet() {
  static TimezonePickerApplet s;
  return s;
}

}  // namespace mishmesh
