// mishmesh/applets/ServerLoginApplet.cpp
#include <mishmesh/applets/ServerLoginApplet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/MsgTypes.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/applets/RepeaterManageApplet.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

void ServerLoginApplet::setTarget(const uint8_t* pubKey, const char* name, Mode mode) {
  memcpy(_pub, pubKey, 6);
  if (name) { copyStr(_name, sizeof(_name), name); }
  else _name[0] = 0;
  _mode = mode;
}

void ServerLoginApplet::onStart(AppletContext& ctx) {
  _ctx = &ctx;
  _host = ctx.host;
  _svc = ctx.contacts;
  _storage = ctx.storage;
  _phase = Phase::Start;
  _submitted = false;
  _freshPw = false;
  _waitStartMs = 0;
}

void ServerLoginApplet::onForeground() {
  // Back-returned from the keypad without an OK -> abort the whole flow.
  if (_phase == Phase::Entering && !_submitted && _host) _host->pop();
}

int ServerLoginApplet::storageKey(char* dst, int cap) const {
  const char* prefix = (_mode == Mode::Repeater) ? "repw" : "rmpw";
  return snprintf(dst, cap, "%s%02x%02x%02x%02x%02x%02x", prefix,
                  _pub[0], _pub[1], _pub[2], _pub[3], _pub[4], _pub[5]);
}

void ServerLoginApplet::promptPassword() {
  _phase = Phase::Entering;
  _submitted = false;
  _pwBuf[0] = 0;
  const char* title = (_mode == Mode::Repeater) ? "Repeater password" : "Room password";
  keypadApplet().configure(_pwBuf, sizeof(_pwBuf) - 1, title,
                           &ServerLoginApplet::onPwDone, this);
  if (_host) _host->push(&keypadApplet());
}

void ServerLoginApplet::onPwDone(void* ctx, const char* text) {
  auto* self = static_cast<ServerLoginApplet*>(ctx);
  if (text && text != self->_pwBuf) {
    copyStr(self->_pwBuf, sizeof(self->_pwBuf), text);
  }
  self->_submitted = true;
  self->_freshPw = true;      // typed by hand -> may offer to remember on success
  self->startLogin();
}

void ServerLoginApplet::startLogin() {
  _phase = Phase::WaitingLogin;
  _waitStartMs = 0;
  _seqStart = _svc ? _svc->loginSeq() : 0;
  if (_svc) _svc->login(_pub, _pwBuf);
}

void ServerLoginApplet::onSuccess() {
  if (_mode == Mode::Repeater) {
    repeaterManageApplet().setTarget(_pub, _name);
    if (_host) _host->replace(&repeaterManageApplet());
    return;
  }
  messageThreadApplet().setTarget(directKey(_pub), _name);
  if (_host) _host->replace(&messageThreadApplet());
}

int ServerLoginApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();

  if (_phase == Phase::Start) {
    // First frame: pick the path (host navigation is safe here, as in
    // NotificationApplet's auto-dismiss).
    if (_svc && _svc->isLoggedIn(_pub)) { onSuccess(); return 0; }
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
  const char* fallback = (_mode == Mode::Repeater) ? "Repeater" : "Room";
  c.drawText(body, w / 2, 6, _name[0] ? _name : fallback, DisplayDriver::LIGHT, TextAlign::Center);
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
          onSuccess();
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

bool ServerLoginApplet::onInput(InputEvent ev) {
  if (_phase == Phase::AskRemember) {
    if (_remember.onInput(ev)) {
      ConfirmResult r = _remember.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _storage) {
          char key[24];
          storageKey(key, sizeof(key));
          _storage->save(key, (const uint8_t*)_pwBuf, (uint8_t)strlen(_pwBuf));
        }
        onSuccess();
      }
    }
    return true;   // modal: swallow input while the dialog is up
  }
  return false;    // WaitingLogin: let Back bubble so the host pops us (cancel)
}

ServerLoginApplet& serverLoginApplet() {
  static ServerLoginApplet s_serverLogin;
  return s_serverLogin;
}

}  // namespace mishmesh
