// mishmesh/applets/RepeaterNeighborsApplet.cpp
#include <mishmesh/applets/RepeaterNeighborsApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/CliRequest.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

void RepeaterNeighborsApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterNeighborsApplet::appendMultiline(const char* s) {
  if (!s || !s[0]) { _view.addLine(""); return; }
  char seg[64]; int n = 0;
  for (const char* p = s; ; p++) {
    if (*p == '\n' || *p == 0) {
      seg[n < 64 ? n : 63] = 0; _view.addLine(seg); n = 0;
      if (*p == 0) break;
    } else if (n < 63) seg[n++] = *p;
  }
}

void RepeaterNeighborsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.contacts; _app = ctx.app;
  _view.setWrap(true);
  _view.clear();
  _view.addLine("Loading...");
  _pending = false;
  if (_svc) {
    _startSeq = cliFire(_svc, _req, _pub, "neighbors", _host ? _host->nowMs() : 0, 20000);
    _pending = true;
  }
  if (_host) _host->requestRender();
}

int RepeaterNeighborsApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();

  if (_pending && _svc) {
    bool ok = false; const char* resp = nullptr;
    CliPoll s = cliPoll(_svc, _req, _pub, _startSeq, c.now(), ok, resp);
    if (s == CliPoll::Ready) {
      _view.clear(); appendMultiline(resp); _pending = false;
    } else if (s == CliPoll::TimedOut) {
      _view.clear(); _view.addLine("[no response]"); _pending = false;
    }
  }

  int bh = drawTopBar(c, _bar, _name[0] ? _name : "Neighbors", _app, w);
  int top = bh + 1;
  _view.draw(c, 0, top, w, h - top);

  if (_pending) return 150;
  return _view.needsAnimation() ? 33 : 1000;
}

bool RepeaterNeighborsApplet::onInput(InputEvent ev) {
  if (_view.onInput(ev)) return true;   // scroll
  return false;                         // Back bubbles
}

RepeaterNeighborsApplet& repeaterNeighborsApplet() {
  static RepeaterNeighborsApplet s_neighbors;
  return s_neighbors;
}

}  // namespace mishmesh
