// mishmesh/applets/MessagePathApplet.cpp
#include "MessagePathApplet.h"
#include <mishmesh/core/Canvas.h>
#include <cstdio>

namespace mishmesh {

void MessagePathApplet::onStart(AppletContext& ctx) {
  _svc = ctx.messages;
  rebuild();
}

void MessagePathApplet::rebuild() {
  _text.clear();
  _rows = 0;
  if (!_svc) return;
  MessageView m;
  if (!_svc->getMessage(_key, _idx, m)) return;
  if (m.kind == KIND_OUT_CHAN) {
    int rc = _svc->repeatCount(_key, _idx);
    if (rc == 0) {
      char buf[32];
      snprintf(buf, sizeof(buf), "Heard %u Repeats", (unsigned)m.heardCount);
      _text.addLine(buf);
      _text.addLine("(detail unavailable)");
      return;
    }
    for (int r = 0; r < rc; r++) {
      RepeatView rv;
      if (!_svc->getRepeat(_key, _idx, r, rv)) continue;
      _text.addf("%s  %uh  %.1fdB", rv.repeaterName, (unsigned)rv.hops,
                 (double)rv.snrx4 / 4.0);
      _rows++;
    }
  } else {
    // inbound (or outbound DM with no path): show arrival hops
    for (int h = 0; h < m.pathLen; h++) {
      const char* name = "";
      uint8_t kc = 0;
      _svc->resolveHop(m.path[h], name, kc);
      _text.addf("Hop %d: %s", h + 1, (name && name[0]) ? name : "?");
      _rows++;
    }
    if (m.pathLen == 0) _text.addLine("Direct (no path)");
  }
}

int MessagePathApplet::onRender(Canvas& c) {
  _text.draw(c, 0, 0, c.width(), c.height());
  return 500;
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
