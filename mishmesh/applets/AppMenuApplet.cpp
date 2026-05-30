#include <mishmesh/applets/AppMenuApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void AppMenuApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _count = 0;
  for (const AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->placement != Placement::AppMenu || _count >= MAX_ENTRIES) continue;
    int i = _count++;
    while (i > 0 && _entries[i - 1]->order > r->order) {
      _entries[i] = _entries[i - 1];
      i--;
    }
    _entries[i] = r;
  }
  _list.setRowHeight(14);
  _list.setModel(this);
}

int AppMenuApplet::onRender(Canvas& c) {
  int w = c.width();
  const int headH = c.fontHeight(fontBody()) + 3;
  int ty = (headH - 1 - c.fontHeight(fontBody())) / 2;
  if (ty < 0) ty = 0;
  c.drawText(fontBody(), 4, ty, "Apps", DisplayDriver::LIGHT);
  c.fillRect(0, headH - 1, w, 1, DisplayDriver::LIGHT);
  _list.draw(c, 0, headH, w, c.height() - headH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool AppMenuApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _count > 0 && _host) {
    _host->push(_entries[_list.selected()]->applet);
    return true;
  }
  return false;
}

}  // namespace mishmesh
