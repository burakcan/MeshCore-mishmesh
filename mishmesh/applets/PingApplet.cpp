#include <mishmesh/applets/PingApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const uint32_t PING_TIMEOUT_MS = 12000;

PingApplet::PingApplet()
    : Applet("Ping"), _host(nullptr), _svc(nullptr),
      _startSeq(0), _startMs(0), _done(false) {
  _pubkey[0] = 0; _result.replied = false; _result.rttMs = 0;
}

void PingApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _done = false;
  _result.replied = false; _result.rttMs = 0;
  _startSeq = _svc ? _svc->pingSeq() : 0;
  _startMs = 0;   // set on first render via Canvas::now()
  if (_svc) _svc->ping(_pubkey);
}

int PingApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  if (_startMs == 0) _startMs = c.now();
  c.drawText(fontBody(), 2, 0, "Ping", DisplayDriver::LIGHT);
  c.fillRect(0, c.lineHeight(fontBody()), w, 1, DisplayDriver::LIGHT);
  int bodyY = c.lineHeight(fontBody()) + 4;

  if (!_done && _svc && _svc->pingSeq() != _startSeq) {
    _svc->latestPing(_pubkey, _result);
    _done = true;
  }

  char line[28];
  if (_done) {
    if (_result.replied) snprintf(line, sizeof(line), "Reply: %u ms", _result.rttMs);
    else                 snprintf(line, sizeof(line), "No reply");
    c.drawText(fontBody(), 2, bodyY, line, DisplayDriver::LIGHT);
    return 1000;
  }

  bool timedOut = (c.now() - _startMs) > PING_TIMEOUT_MS;
  if (timedOut) { c.drawText(fontBody(), 2, bodyY, "No response", DisplayDriver::LIGHT); return 1000; }
  c.drawText(fontBody(), 2, bodyY, "Pinging...", DisplayDriver::LIGHT);
  return 250;
}

bool PingApplet::onInput(InputEvent) {
  return false;   // Back bubbles -> pop
}

PingApplet& pingApplet() {
  static PingApplet s_ping;
  return s_ping;
}
void pingSetTarget(const uint8_t* pubKey) { pingApplet().setTarget(pubKey); }

}  // namespace mishmesh
