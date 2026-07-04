// mishmesh/applets/RepeaterManageApplet.cpp
#include <mishmesh/applets/RepeaterManageApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/CommandLineApplet.h>
#include <mishmesh/applets/StatusApplet.h>
#include <mishmesh/applets/RepeaterSettingsApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

void RepeaterManageApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterManageApplet::activateTab(int t) {
  _tab = t;
  _tabs.setSelected(t);
  if (!_ctx) return;
  switch (t) {
    case 0: statusApplet().onShow(*_ctx); break;
    case 1: commandLineApplet().onShow(*_ctx); break;
    default: repeaterSettingsApplet().onShow(*_ctx); break;
  }
}

void RepeaterManageApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app  = ctx.app;
  _ctxStore = ctx;
  _ctx  = &_ctxStore;

  // Forward target to all three views so their CLI/status calls go to the right node.
  statusApplet().setTarget(_pub, _name);
  commandLineApplet().setTarget(_pub, _name);
  repeaterSettingsApplet().setTarget(_pub, _name);

  // Full reset of embedded views (same state as if each were pushed standalone).
  commandLineApplet().onStart(ctx);          // clears log, resets pending
  repeaterSettingsApplet().onStart(ctx);     // resets menu selection

  _tabs.clear();
  _tabs.addTab("Status",   (uint16_t)Icon::Radio);
  _tabs.addTab("Cmd",      (uint16_t)Icon::Comment);
  _tabs.addTab("Settings", (uint16_t)Icon::Settings);

  // Land on the Status tab and auto-fire the first status request.
  activateTab(0);
}

int RepeaterManageApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  static const int BAR_H = 13;
  _tabs.setBattery(_app ? _app->batteryMillivolts() : 0);
  _tabs.draw(c, 0, 0, w, BAR_H);
  int bodyY = BAR_H + 1;
  int bodyH = h - bodyY;
  switch (_tab) {
    case 0: return statusApplet().renderBody(c, 0, bodyY, w, bodyH);
    case 1: return commandLineApplet().renderBody(c, 0, bodyY, w, bodyH);
    default: return repeaterSettingsApplet().renderBody(c, 0, bodyY, w, bodyH);
  }
}

bool RepeaterManageApplet::onInput(InputEvent ev) {
  // Tab bar handles NavLeft/NavRight; on switch activate the new view.
  if (_tabs.onInput(ev)) {
    activateTab(_tabs.selected());
    return true;
  }
  // Delegate everything else to the active embedded view.
  switch (_tab) {
    case 0: if (statusApplet().onInput(ev)) return true; break;
    case 1: if (commandLineApplet().onInput(ev)) return true; break;
    default: if (repeaterSettingsApplet().onInput(ev)) return true; break;
  }
  return false;   // Back bubbles: host pops us back to contact detail
}

RepeaterManageApplet& repeaterManageApplet() {
  static RepeaterManageApplet s_repeaterManage;
  return s_repeaterManage;
}

}  // namespace mishmesh
