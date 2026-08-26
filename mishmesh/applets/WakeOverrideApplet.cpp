#include <mishmesh/applets/WakeOverrideApplet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

WakeOverrideApplet::WakeOverrideApplet() : Applet("Screen wake") {}

void WakeOverrideApplet::setTarget(const ConvoKey& key, const char* name) {
  _key = key;
  copyStr(_name, sizeof(_name), name ? name : "");
}

const char* WakeOverrideApplet::label(int i) const {
  switch (i) {
    case 0:  return "Default";
    case 1:  return "On";
    case 2:  return "Off";
    default: return "";
  }
}

bool WakeOverrideApplet::radioOn(int i) const {
  return _svc && (int)_svc->chatWake(_key) == i;
}

void WakeOverrideApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _svc = ctx.messages;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
  for (int i = 0; i < count(); i++) if (radioOn(i)) { _list.setSelected(i); break; }
}

int WakeOverrideApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle(_name);
  if (_app) _bar.setBattery(_app->batteryMillivolts());
  _bar.draw(c, 0, 0, w, barH);
  _list.draw(c, 0, barH, w, h - barH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool WakeOverrideApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _svc) {
    _svc->setChatWake(_key, (WakeOverride)_list.selected());
    return true;   // radio mark refreshes from chatWake() next render
  }
  return false;    // Back bubbles -> host pops
}

WakeOverrideApplet& wakeOverrideApplet() {
  static WakeOverrideApplet a;
  return a;
}

}  // namespace mishmesh
