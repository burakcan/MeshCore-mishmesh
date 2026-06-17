// mishmesh/applets/ChatNotifyApplet.cpp
#include "ChatNotifyApplet.h"
#include <mishmesh/applets/SoundPickerApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

ChatNotifyApplet::ChatNotifyApplet() : Applet("Notify settings") {}

void ChatNotifyApplet::setTarget(const ConvoKey& key, const char* name) {
  _key = key;
  _isChannel = (key.type == 1);
  strncpy(_name, name ? name : "", sizeof(_name) - 1);
  _name[sizeof(_name) - 1] = 0;
}

void ChatNotifyApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _svc = ctx.messages; _host = ctx.host;
  _level = _svc ? _svc->notifyLevel(_key) : NotifyLevel::All;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
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
  if (ev == InputEvent::Select) {
    int sel = _list.selected();
    if (soundRow(sel)) {
      soundPickerApplet().setChat(_key, _name);
      if (_host) _host->push(&soundPickerApplet());
    } else {
      _level = levelForRow(sel);
      if (_svc) _svc->setNotifyLevel(_key, _level);
    }
    return true;
  }
  return false;   // Back bubbles -> host pops
}

const char* ChatNotifyApplet::label(int i) const {
  if (soundRow(i)) return "Sound";
  if (_isChannel) {
    if (i == 0) return "All";
    if (i == 1) return "Mentions only";
    return "Mute";
  } else {
    if (i == 0) return "All";
    return "Mute";
  }
}

const char* ChatNotifyApplet::value(int i) const {
  if (!soundRow(i)) return nullptr;
  uint8_t e = _svc ? _svc->chatSound(_key) : 0;
  return sound::notifyToneEncodedName(e, /*perChat*/true, sound::notifyTypeDefault(_isChannel));
}

bool ChatNotifyApplet::isRadio(int i) const {
  return !soundRow(i);
}

bool ChatNotifyApplet::radioOn(int i) const {
  if (soundRow(i)) return false;
  return _level == levelForRow(i);
}

NotifyLevel ChatNotifyApplet::levelForRow(int row) const {
  if (_isChannel) {
    if (row == 0) return NotifyLevel::All;
    if (row == 1) return NotifyLevel::MentionsOnly;
    return NotifyLevel::Mute;
  } else {
    if (row == 0) return NotifyLevel::All;
    return NotifyLevel::Mute;
  }
}

ChatNotifyApplet& chatNotifyApplet() {
  static ChatNotifyApplet a;
  return a;
}

}  // namespace mishmesh
