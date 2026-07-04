// mishmesh/applets/RepeaterIdentityApplet.cpp
#include <mishmesh/applets/RepeaterIdentityApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/Modal.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

void RepeaterIdentityApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterIdentityApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc  = ctx.contacts;
  _app  = ctx.app;
  _phase = Phase::Menu;
  _seedReady = false;
  _keyHex[0] = 0;
  _status[0] = 0;
  static const char* const MENU_LABELS[] = {"Generate random", "Enter seed"};
  _menuModel.set(MENU_LABELS, 2);
  _menu.setModel(&_menuModel);
  _menu.resetSelection();
}

void RepeaterIdentityApplet::onForeground() {
  if (_phase == Phase::Enter) {
    if (_seedReady) {
      _seedReady = false;
      if (_svc && _svc->makeIdentityHex(_seedBuf, _keyHex, 129)) {
        buildScrollText();
        _phase = Phase::Show;
      } else {
        _keyHex[0] = 0;
        if (_host) _host->postToast("Bad seed (need 64 hex)");
        _phase = Phase::Menu;
      }
    } else {
      _phase = Phase::Menu;   // user backed out of keypad
    }
  }
}

void RepeaterIdentityApplet::buildScrollText() {
  _scroll.clear();
  // 128 hex chars as 8 lines of 16 chars each
  char buf[17];
  for (int line = 0; line < 8; line++) {
    memcpy(buf, _keyHex + line * 16, 16);
    buf[16] = 0;
    _scroll.addLine(buf);
  }
  _scroll.addLine("Select to apply");
}

void RepeaterIdentityApplet::doGenerate() {
  if (_svc && _svc->makeIdentityHex(nullptr, _keyHex, 129)) {
    buildScrollText();
    _phase = Phase::Show;
  } else {
    if (_host) _host->postToast("Keygen failed");
  }
  if (_host) _host->requestRender();
}

void RepeaterIdentityApplet::doSetKey() {
  memcpy(_cmd, "set prv.key ", 12);
  memcpy(_cmd + 12, _keyHex, 128);
  _cmd[140] = 0;
  _phase = Phase::Busy;
  _startSeq = _svc ? _svc->cliSeq() : 0;
  _req.timeoutMs = 20000;
  _req.begin(_startSeq, _host ? _host->nowMs() : 0);
  if (_svc) _svc->sendCliCommand(_pub, _cmd);
  if (_host) _host->requestRender();
}

/*static*/ void RepeaterIdentityApplet::onSeedConfirm(void* ctx, const char* text) {
  auto* self = static_cast<RepeaterIdentityApplet*>(ctx);
  if (text && text != self->_seedBuf) {
    strncpy(self->_seedBuf, text, 64);
    self->_seedBuf[64] = 0;
  } else if (!text) {
    self->_seedBuf[0] = 0;
  }
  self->_seedReady = true;
}

int RepeaterIdentityApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();

  if (_phase == Phase::Busy && _svc) {
    uint32_t seq = _svc->cliSeq();
    PendingRequest::State st = _req.poll(seq, c.now());
    if (st == PendingRequest::State::Ready) {
      bool ok = false; const char* resp = nullptr;
      if (_svc->cliResult(_pub, _startSeq, ok, resp)) {
        if (resp && resp[0] == 'O' && resp[1] == 'K') {
          const char* p = strstr(resp, "New pubkey: ");
          if (p) p += 12;
          snprintf(_status, sizeof(_status), "%.64s", p ? p : "");
          if (_svc) _svc->sendCliCommand(_pub, "reboot");   // fire-and-forget
          _phase = Phase::Done;
        } else {
          if (_host) _host->postToast("Apply failed");
          _phase = Phase::Menu;
        }
      } else {
        _req.rearm(seq);
      }
    } else if (st == PendingRequest::State::TimedOut) {
      if (_host) _host->postToast("Apply failed");
      _phase = Phase::Menu;
    }
  }

  int bh = drawTopBar(c, _bar, _name[0] ? _name : "Identity", _app, w);
  int top = bh + 1;

  if (_phase == Phase::Menu) {
    _menu.draw(c, 0, top, w, h - top);
  } else if (_phase == Phase::Show || _phase == Phase::Confirm) {
    Canvas modal = drawModalChrome(c);
    _scroll.draw(modal, 0, 0, modal.width(), modal.height());
    if (_phase == Phase::Confirm) _confirm.draw(c, 0, 0, w, h);
  } else if (_phase == Phase::Busy) {
    const Font* cap = fontCaption();
    c.drawText(cap, w / 2, top + (h - top) / 2, "Working...", DisplayDriver::LIGHT, TextAlign::Center);
    return 150;
  } else if (_phase == Phase::Done) {
    const Font* cap = fontCaption();
    int mid = top + (h - top) / 2;
    c.drawText(cap, w / 2, mid - 4, "Rebooting...", DisplayDriver::LIGHT, TextAlign::Center);
    if (_status[0]) c.drawText(cap, w / 2, mid + 8, _status, DisplayDriver::LIGHT, TextAlign::Center);
  }

  if (_phase == Phase::Menu && _menu.needsAnimation()) return ListMenu::TICK_MS;
  return 1000;
}

bool RepeaterIdentityApplet::onInput(InputEvent ev) {
  if (_phase == Phase::Confirm) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r == ConfirmResult::Confirmed)  { doSetKey(); }
      else if (r == ConfirmResult::Cancelled) { _phase = Phase::Show; }
    }
    return true;   // all input consumed while confirming
  }
  if (_phase == Phase::Show) {
    if (_scroll.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      _confirm.configure("Replace repeater key + reboot? You must re-add it.");
      _phase = Phase::Confirm;
      return true;
    }
    if (ev == InputEvent::Back) { _phase = Phase::Menu; return true; }
    return true;   // modal: consume everything else
  }
  if (_phase == Phase::Busy) return true;    // block all input while working
  if (_phase == Phase::Done) return false;   // Back bubbles to pop
  // Menu phase
  if (_menu.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int sel = _menu.selected();
    if (sel == 0) {
      doGenerate();
    } else {
      _kpBuf[0] = 0; _seedBuf[0] = 0; _seedReady = false;
      _phase = Phase::Enter;
      keypadApplet().configure(_kpBuf, 64, "Seed (64 hex)", &RepeaterIdentityApplet::onSeedConfirm, this);
      if (_host) _host->push(&keypadApplet());
    }
    return true;
  }
  return false;   // Back bubbles
}

RepeaterIdentityApplet& repeaterIdentityApplet() {
  static RepeaterIdentityApplet s_inst;
  return s_inst;
}

}  // namespace mishmesh
