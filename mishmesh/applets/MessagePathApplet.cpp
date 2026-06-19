// mishmesh/applets/MessagePathApplet.cpp
#include "MessagePathApplet.h"
#include <mishmesh/core/Canvas.h>
#include <mishmesh/widgets/ListMenu.h>   // TICK_MS
#include <cstdio>

namespace mishmesh {

void MessagePathApplet::onStart(AppletContext& ctx) {
  _svc = ctx.messages;
  _app = ctx.app;
  rebuild();
}

void MessagePathApplet::hopLabel(uint8_t b, char* out, size_t cap) const {
  const char* name = ""; uint8_t kc = 0;
  if (_svc && _svc->resolveHop(b, name, kc) && name && name[0])
    snprintf(out, cap, "%s", name);
  else
    snprintf(out, cap, "%02X", b);
}

void MessagePathApplet::rebuild() {
  _text.clear();
  _rows = 0;
  _title[0] = 0;
  _bar.setTitle(_title);
  if (!_svc) return;
  MessageView m;
  if (!_svc->getMessage(_key, _idx, m)) return;

  if (m.kind == KIND_OUT_CHAN) {
    if (m.heardCount == 0)      snprintf(_title, sizeof(_title), "Heard repeats");
    else if (m.heardCount == 1) snprintf(_title, sizeof(_title), "Heard 1 time");
    else                        snprintf(_title, sizeof(_title), "Heard %u times", (unsigned)m.heardCount);
    _bar.setTitle(_title);
    int rc = _svc->repeatCount(_key, _idx);
    if (rc == 0) {
      _text.addLine(m.heardCount ? "Details not available" : "Not heard yet");
      return;
    }
    for (int r = 0; r < rc; r++) {
      RepeatView rv;
      if (!_svc->getRepeat(_key, _idx, r, rv)) continue;
      _text.addf("#%d   %uh | %.1fdB", r + 1, (unsigned)rv.hops, (double)rv.snrx4 / 4.0);
      // The repeater we actually heard this echo from is the LAST hop; the path
      // accumulates origin-first, so path[0] is always our nearest repeater and
      // uninformative here.
      if (rv.pathLen > 0) {
        char lbl[34]; hopLabel(rv.path[rv.pathLen - 1], lbl, sizeof(lbl));
        _text.addf("  %s", lbl);
      } else {
        _text.addLine("  (direct)");
      }
      _rows++;
    }
  } else {
    // inbound (or outbound DM with no path): the flood relay chain
    if (m.pathLen == 0)   snprintf(_title, sizeof(_title), "Direct");
    else if (m.hops == 1) snprintf(_title, sizeof(_title), "Path: 1 hop");
    else                  snprintf(_title, sizeof(_title), "Path: %u hops", (unsigned)m.hops);
    _bar.setTitle(_title);
    for (int h = 0; h < m.pathLen; h++) {
      char lbl[20]; hopLabel(m.path[h], lbl, sizeof(lbl));
      _text.addf("%d  %s", h + 1, lbl);
      _rows++;
    }
    if (m.pathLen == 0) _text.addLine("Direct (no path)");
  }
}

int MessagePathApplet::onRender(Canvas& c) {
  int bw = 0, bh = 0; _bar.measure(bw, bh);
  _bar.setBattery(_app ? _app->batteryMillivolts() : 0);
  _bar.draw(c, 0, 0, c.width(), bh);
  Canvas body = c.region(0, bh, c.width(), c.height() - bh);
  _text.draw(body, 0, 0, body.width(), body.height());
  return _text.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool MessagePathApplet::onInput(InputEvent ev) {
  if (_text.onInput(ev)) return true;
  return false;  // Back -> bubbles up -> pops
}

MessagePathApplet& messagePathApplet() {
  static MessagePathApplet a;
  return a;
}

}  // namespace mishmesh
