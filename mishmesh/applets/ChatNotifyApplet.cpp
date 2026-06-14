#include <mishmesh/applets/ChatNotifyApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

// Row order is fixed; DMs omit "Mentions only" (a 1:1 DM is implicitly to you).
// Channel rows: 0=All, 1=Mentions only, 2=Mute. DM rows: 0=All, 1=Mute.
ChatNotifyApplet::ChatNotifyApplet() : Applet("Notify settings") {}

void ChatNotifyApplet::setTarget(const ConvoKey& key, const char* name) {
  _key = key;
  _isChannel = (key.type == 1);
  strncpy(_name, name ? name : "", sizeof(_name) - 1);
  _name[sizeof(_name) - 1] = 0;
}

NotifyLevel ChatNotifyApplet::levelForRow(int row) const {
  if (_isChannel) {
    if (row == 0) return NotifyLevel::All;
    if (row == 1) return NotifyLevel::MentionsOnly;
    return NotifyLevel::Mute;
  }
  return row == 0 ? NotifyLevel::All : NotifyLevel::Mute;
}

const char* ChatNotifyApplet::label(int i) const {
  if (_isChannel) {
    if (i == 0) return "All";
    if (i == 1) return "Mentions only";
    return "Mute";
  }
  return i == 0 ? "All" : "Mute";
}

bool ChatNotifyApplet::radioOn(int i) const {
  return levelForRow(i) == _level;
}

void ChatNotifyApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _svc = ctx.messages;
  _level = _svc ? _svc->notifyLevel(_key) : NotifyLevel::All;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();   // shared singleton: always reopen at the first row
}

int ChatNotifyApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle(_name);
  if (_app) _bar.setBattery(_app->batteryMillivolts());
  _bar.draw(c, 0, 0, w, barH);
  _list.draw(c, 0, barH, w, h - barH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool ChatNotifyApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _svc) {
    _level = levelForRow(_list.selected());
    _svc->setNotifyLevel(_key, _level);   // persist; toggles mirror on next render
    return true;
  }
  return false;   // Back bubbles -> host pops back to the chat menu
}

ChatNotifyApplet& chatNotifyApplet() {
  static ChatNotifyApplet s_applet;
  return s_applet;
}

}  // namespace mishmesh
