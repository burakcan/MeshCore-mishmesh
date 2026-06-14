#include <mishmesh/applets/AppMenuApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
// [mishmesh]
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/applets/MessagesApplet.h>
#include <stdio.h>
// [/mishmesh]

namespace mishmesh {

void AppMenuApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  // [mishmesh]
  _ctx_messages = ctx.messages;
  // [/mishmesh]
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
  // const int headH = c.fontHeight(fontBody()) + 3;
  // int ty = (headH - 1 - c.fontHeight(fontBody())) / 2;
  // if (ty < 0) ty = 0;
  // c.drawText(fontBody(), 4, ty, "Apps", DisplayDriver::LIGHT);
  // c.fillRect(0, headH - 1, w, 1, DisplayDriver::LIGHT);
  // _list.draw(c, 0, headH, w, c.height() - headH);
  _list.draw(c, 0, 0, w, c.height());
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

// [mishmesh] unread badge: return count string for the Messages row, nullptr otherwise
const char* AppMenuApplet::value(int i) const {
  if (!_ctx_messages || i < 0 || i >= _count) return nullptr;
  if (_entries[i]->applet != &messagesApplet()) return nullptr;
  uint16_t u = _ctx_messages->totalNotifyUnread();   // global badge excludes muted/non-mention
  if (!u) return nullptr;
  static char buf[8];
  snprintf(buf, sizeof(buf), "%u", u);
  return buf;
}
// [/mishmesh]

}  // namespace mishmesh
