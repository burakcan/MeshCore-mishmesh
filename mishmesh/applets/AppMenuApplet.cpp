#include <mishmesh/applets/AppMenuApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>

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
  _list.setModel(this);
}

int AppMenuApplet::onRender(Canvas& c) {
  int w = c.width();
  c.text(2, 0, "Apps", DisplayDriver::LIGHT);
  c.fillRect(0, 11, w, 1, DisplayDriver::LIGHT);
  _list.draw(c, 0, 14, w, c.height() - 14);
  return 1000;
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
