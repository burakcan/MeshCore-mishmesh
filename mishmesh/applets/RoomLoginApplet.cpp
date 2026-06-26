// mishmesh/applets/RoomLoginApplet.cpp
#include <mishmesh/applets/RoomLoginApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/MsgTypes.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

void RoomLoginApplet::setTarget(const uint8_t* pubKey, const char* name) {
  memcpy(_pub, pubKey, 6);
  if (name) { strncpy(_name, name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = 0; }
  else _name[0] = 0;
}

void RoomLoginApplet::onStart(AppletContext& ctx) {
  _ctx = &ctx;
  _host = ctx.host;
  _svc = ctx.contacts;
  _storage = ctx.storage;
  _phase = Phase::Start;
  _submitted = false;
  _freshPw = false;
  _waitStartMs = 0;
}

void RoomLoginApplet::onForeground() {
  // Back-returned from the keypad without an OK -> abort the whole flow.
  if (_phase == Phase::Entering && !_submitted && _host) _host->pop();
}

int RoomLoginApplet::storageKey(char* dst, int cap) const {
  return snprintf(dst, cap, "rmpw%02x%02x%02x%02x%02x%02x",
                  _pub[0], _pub[1], _pub[2], _pub[3], _pub[4], _pub[5]);
}

void RoomLoginApplet::promptPassword() {
  _phase = Phase::Entering;
  _submitted = false;
  _pwBuf[0] = 0;
  keypadApplet().configure(_pwBuf, sizeof(_pwBuf) - 1, "Room password",
                           &RoomLoginApplet::onPwDone, this);
  if (_host) _host->push(&keypadApplet());
}

void RoomLoginApplet::onPwDone(void* ctx, const char* text) {
  auto* self = static_cast<RoomLoginApplet*>(ctx);
  if (text && text != self->_pwBuf) {
    strncpy(self->_pwBuf, text, sizeof(self->_pwBuf) - 1);
    self->_pwBuf[sizeof(self->_pwBuf) - 1] = 0;
  }
  self->_submitted = true;
  self->_freshPw = true;      // typed by hand -> may offer to remember on success
  self->startLogin();
}

void RoomLoginApplet::startLogin() {
  _phase = Phase::WaitingLogin;
  _waitStartMs = 0;
  _seqStart = _svc ? _svc->loginSeq() : 0;
  if (_svc) _svc->login(_pub, _pwBuf);
}

void RoomLoginApplet::openThread() {
  messageThreadApplet().setTarget(directKey(_pub), _name);
  if (_host) _host->replace(&messageThreadApplet());
}

int RoomLoginApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();

  if (_phase == Phase::Start) {
    // First frame: pick the path (host navigation is safe here, as in
    // NotificationApplet's auto-dismiss).
    if (_svc && _svc->isLoggedIn(_pub)) { openThread(); return 0; }
    char key[24];
    storageKey(key, sizeof(key));
    uint8_t n = _storage ? _storage->load(key, (uint8_t*)_pwBuf, sizeof(_pwBuf) - 1) : 0;
    if (n > 0) {
      _pwBuf[n] = 0;
      _freshPw = false;
      _submitted = true;
      startLogin();             // -> WaitingLogin, fall through to draw
    } else {
      promptPassword();         // -> Entering, keypad now on top
      return 50;
    }
  }

  if (_phase == Phase::Entering) return 200;   // keypad draws itself on top

  // WaitingLogin / AskRemember share this "connecting" card.
  const Font* body = fontBody();
  const Font* cap = fontCaption();
  int lh = c.lineHeight(body);
  c.drawText(body, w / 2, 6, _name[0] ? _name : "Room", DisplayDriver::LIGHT, TextAlign::Center);
  c.drawText(cap, w / 2, 6 + lh + 4, "Logging in...", DisplayDriver::LIGHT, TextAlign::Center);
  c.drawText(cap, w / 2, h - c.lineHeight(cap) - 2, "Back to cancel",
             DisplayDriver::LIGHT, TextAlign::Center);

  if (_phase == Phase::WaitingLogin) {
    if (_waitStartMs == 0) _waitStartMs = c.now();
    uint32_t seq = _svc ? _svc->loginSeq() : _seqStart;
    if (seq != _seqStart) {
      bool ok = false, admin = false;
      uint8_t perms = 0;
      if (_svc && _svc->loginResult(_pub, ok, admin, perms)) {
        if (ok) {
          if (_freshPw && _storage) {
            _remember.configure("Save password?");
            _phase = Phase::AskRemember;
            return 100;
          }
          openThread();
          return 0;
        }
        if (_host) _host->postToast("Login failed");
        promptPassword();       // let them re-enter
        return 50;
      }
      _seqStart = seq;          // a result for a different contact; keep waiting
    } else if (c.now() - _waitStartMs > LOGIN_TIMEOUT_MS) {
      if (_host) { _host->postToast("No response"); _host->pop(); }
      return 0;
    }
    return 150;
  }

  // AskRemember: overlay the confirm dialog atop the card.
  _remember.draw(c, 0, 0, w, h);
  return 100;
}

bool RoomLoginApplet::onInput(InputEvent ev) {
  if (_phase == Phase::AskRemember) {
    if (_remember.onInput(ev)) {
      ConfirmResult r = _remember.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _storage) {
          char key[24];
          storageKey(key, sizeof(key));
          _storage->save(key, (const uint8_t*)_pwBuf, (uint8_t)strlen(_pwBuf));
        }
        openThread();
      }
    }
    return true;   // modal: swallow input while the dialog is up
  }
  return false;    // WaitingLogin: let Back bubble so the host pops us (cancel)
}

RoomLoginApplet& roomLoginApplet() {
  static RoomLoginApplet s_roomLogin;
  return s_roomLogin;
}

}  // namespace mishmesh
