// mishmesh/applets/RepeaterTelemetryApplet.cpp
#include <mishmesh/applets/RepeaterTelemetryApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <string.h>

namespace mishmesh {

void RepeaterTelemetryApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterTelemetryApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.contacts;
  _done = false; _startMs = 0;
  _dlg.setWaiting();
  _startSeq = _svc ? _svc->telemetrySeq() : 0;
  if (_svc) _svc->requestTelemetry(_pub);
  if (_host) _host->requestRender();
}

int RepeaterTelemetryApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  if (!_done && _svc) {
    if (_startMs == 0) _startMs = c.now();
    TelemetryView v;
    if (_svc->telemetrySeq() != _startSeq && _svc->latestTelemetry(_pub, v)) {
      _dlg.setResult(v); _done = true;
    } else if (c.now() - _startMs > TELEM_TIMEOUT_MS) {
      _dlg.setTimeout(); _done = true;
    }
  }
  _dlg.draw(c, 0, 0, w, h);
  if (_dlg.needsAnimation()) return ListMenu::TICK_MS;
  return _done ? 1000 : 200;
}

bool RepeaterTelemetryApplet::onInput(InputEvent ev) {
  if (_dlg.onInput(ev)) return true;   // scroll
  return false;                        // Back bubbles -> host pops to the hub
}

RepeaterTelemetryApplet& repeaterTelemetryApplet() {
  static RepeaterTelemetryApplet s_telem;
  return s_telem;
}

}  // namespace mishmesh
