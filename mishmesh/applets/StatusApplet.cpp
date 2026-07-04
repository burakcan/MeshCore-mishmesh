// mishmesh/applets/StatusApplet.cpp
#include <mishmesh/applets/StatusApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

// Compact airtime: "12s" / "5m" / "1h2m" from seconds (RepeaterStats airtime is secs).
static void fmtDurS(char* o, int n, uint32_t s) {
  if (s < 60u)        snprintf(o, n, "%us", (unsigned)s);
  else if (s < 3600u) snprintf(o, n, "%um", (unsigned)(s / 60u));
  else                snprintf(o, n, "%uh%um", (unsigned)(s / 3600u), (unsigned)((s / 60u) % 60u));
}

int formatRepeaterStatus(const RepeaterStatusView& s, uint32_t loginClockEpoch,
                         char out[][STATUS_LINE_LEN], int maxLines) {
  int n = 0;
  char a[12], b[12];
#define RS_EMIT(...) do { if (n < maxLines) snprintf(out[n++], STATUS_LINE_LEN, __VA_ARGS__); } while (0)
  if (!s.valid) { RS_EMIT("No data"); return n; }
  RS_EMIT("Uptime: %uh %02um", (unsigned)(s.upTimeSecs / 3600u), (unsigned)((s.upTimeSecs / 60u) % 60u));
  RS_EMIT("Battery: %u.%02uV", (unsigned)(s.battMilliVolts / 1000u), (unsigned)((s.battMilliVolts % 1000u) / 10u));
  fmtDurS(a, sizeof(a), s.airTxSecs); fmtDurS(b, sizeof(b), s.airRxSecs);
  RS_EMIT("Air TX %s RX %s", a, b);
  RS_EMIT("RSSI %d  SNR %.1f dB", (int)s.lastRssi, (double)s.lastSnrX4 / 4.0);
  RS_EMIT("Noise %d dBm", (int)s.noiseFloor);
  RS_EMIT("Queue: %u", (unsigned)s.txQueueLen);
  RS_EMIT("Sent %u f%u d%u", (unsigned)s.packetsSent, (unsigned)s.sentFlood, (unsigned)s.sentDirect);
  RS_EMIT("Recv %u f%u d%u", (unsigned)s.packetsRecv, (unsigned)s.recvFlood, (unsigned)s.recvDirect);
  RS_EMIT("Dups f%u d%u", (unsigned)s.floodDups, (unsigned)s.directDups);
  RS_EMIT("RxErr: %u", (unsigned)s.recvErrors);
  RS_EMIT("Debug: %u", (unsigned)s.errEvents);
  if (loginClockEpoch == 0) RS_EMIT("Clock: --");
  else RS_EMIT("Clock: %02u:%02u", (unsigned)((loginClockEpoch / 3600u) % 24u),
               (unsigned)((loginClockEpoch / 60u) % 60u));
#undef RS_EMIT
  return n;
}

void StatusApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
  _autoFetchDone = false;   // new target: allow auto-fetch on next onShow
}

void StatusApplet::onStart(AppletContext& ctx) {
  _autoFetchDone = false;
  _host = ctx.host;
  _svc = ctx.contacts;
  _req.timeoutMs = 20000;   // LoRa round-trip; stays under the 30s auto-off
  _refresh.set("Refresh", (uint16_t)Icon::Reload);
  _view.setHeader(&_refresh, 12);
  fetch();
}

// Embedded-mode activation: called by the tab host when this tab becomes visible.
// Auto-fetches only on the first activation per hub session; tab-switch-back is silent.
void StatusApplet::onShow(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _req.timeoutMs = 20000;
  _refresh.set("Refresh", (uint16_t)Icon::Reload);
  _view.setHeader(&_refresh, 12);
  if (!_autoFetchDone) { _autoFetchDone = true; fetch(); }
}

void StatusApplet::fetch() {
  _view.clear();
  _view.addLine("Loading...");
  _pending = false;
  if (_svc) {
    _statusStart = _svc->statusSeq();
    _req.begin(_statusStart, _host ? _host->nowMs() : 0);
    _svc->requestStatus(_pub);
    _pending = true;
  }
  if (_host) _host->requestRender();
}

void StatusApplet::showResult(const RepeaterStatusView& v) {
  char lines[STATUS_MAX_LINES][STATUS_LINE_LEN];
  uint32_t clock = _svc ? _svc->loginClock(_pub) : 0;
  int n = formatRepeaterStatus(v, clock, lines, STATUS_MAX_LINES);
  _view.clear();
  for (int i = 0; i < n; i++) _view.addLine(lines[i]);
}

int StatusApplet::onRender(Canvas& c) {
  return renderBody(c, 0, 0, c.width(), c.height());
}

// Embedded-mode render: draws into a sub-region below the tab host's TabBar.
// Refresh button is the ScrollText header (top of scroll region, scrolls with lines).
int StatusApplet::renderBody(Canvas& c, int x, int y, int w, int h) {
  if (_pending && _svc) {
    PendingRequest::State st = _req.poll(_svc->statusSeq(), c.now());
    if (st == PendingRequest::State::Ready) {
      RepeaterStatusView v;
      if (_svc->latestStatus(_pub, v)) { _pending = false; showResult(v); }
      else _req.rearm(_svc->statusSeq());
    } else if (st == PendingRequest::State::TimedOut) {
      _pending = false;
      _view.clear();
      _view.addLine("No response");
    }
  }

  _refresh.setFocused(true);   // Select always re-requests; show button as always-active
  _view.draw(c, x, y, w, h);

  if (_pending) return 150;
  return _view.needsAnimation() ? 33 : 1000;
}

bool StatusApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select) { fetch(); return true; }   // always re-requests
  if (_view.onInput(ev)) return true;
  return false;   // Back bubbles: host pops to the hub
}

StatusApplet& statusApplet() {
  static StatusApplet s_status;
  return s_status;
}

}  // namespace mishmesh
