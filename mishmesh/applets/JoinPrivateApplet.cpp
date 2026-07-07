#include <mishmesh/applets/JoinPrivateApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

void JoinPrivateApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
}

const char* JoinPrivateApplet::label(int i) const {
  if (i == 0) return "Name";
  if (i == 1) return "Key";
  return "Join";
}
const char* JoinPrivateApplet::value(int i) const {
  if (i == 0) return (_name && _name[0]) ? _name : "(none)";
  if (i == 1) return (_key && _key[0]) ? _key : "(none)";
  return nullptr;
}

int JoinPrivateApplet::onRender(Canvas& c) {
  int w = c.width();
  int bh = drawTopBar(c, _bar, "Join private", _app, w);
  _list.draw(c, 0, bh + 1, w, c.height() - bh - 1);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 750;
}

bool JoinPrivateApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int sel = _list.selected();
    if (sel == 0) {
      keypadApplet().configure(_name, _nameCap - 1, "Name", nullptr, nullptr);
      if (_host) _host->push(&keypadApplet());
    } else if (sel == 1) {
      keypadApplet().configure(_key, _keyCap - 1, "Key (32 hex)", nullptr, nullptr);
      if (_host) _host->push(&keypadApplet());
    } else {   // Join
      if (!_name || !_name[0]) { if (_host) _host->postToast("Name required"); return true; }
      if (!_keyValid || !_keyValid(_key)) { if (_host) _host->postToast("Key: 32 hex chars"); return true; }
      if (_submit && _submit(_ctx)) { if (_host) _host->pop(); }   // submit toasts; pop on success
    }
    return true;
  }
  return false;   // Back bubbles -> host pops (cancel)
}

JoinPrivateApplet& joinPrivateApplet() {
  static JoinPrivateApplet s;
  return s;
}

}  // namespace mishmesh
