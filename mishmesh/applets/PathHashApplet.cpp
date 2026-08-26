#include <mishmesh/applets/PathHashApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

static const char* INFO =
  "1 byte is safest. 2/3 need fw 1.14+ repeaters.";

PathHashApplet::PathHashApplet() : Applet("Path hash size") {}

const char* PathHashApplet::label(int i) const {
  switch (i) {
    case 0: return "1 byte";
    case 1: return "2 byte";
    case 2: return "3 byte";
    default: return "";
  }
}

bool PathHashApplet::radioOn(int i) const {
  return _app && (int)_app->pathHashMode() == i;
}

void PathHashApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
  for (int i = 0; i < count(); i++) if (radioOn(i)) { _list.setSelected(i); break; }
}

void PathHashApplet::InfoHeader::draw(Canvas& c, int x, int y, int w, int h) {
  (void)h;
  c.drawTextWrapped(fontBody(), x + 2, y, w - 4, text, DisplayDriver::LIGHT);
}

int PathHashApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle("Path hash size");
  _bar.setBattery(_app ? _app->batteryMillivolts() : 0);
  _bar.draw(c, 0, 0, w, barH);

  _info.text = INFO;
  _lastCaptionH = c.measureTextWrapped(fontBody(), w - 4, INFO);
  _list.setHeader(&_info, _lastCaptionH);
  int y = barH + 1;
  _lastBodyH = h - y;
  _list.draw(c, 0, y, w, _lastBodyH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool PathHashApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _app) {
    _app->setPathHashMode((uint8_t)_list.selected());
    return true;   // radio mark refreshes from pathHashMode() next render
  }
  return false;    // Back bubbles -> host pops
}

PathHashApplet& pathHashApplet() {
  static PathHashApplet s;
  return s;
}

}  // namespace mishmesh
