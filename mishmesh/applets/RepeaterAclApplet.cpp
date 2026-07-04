// mishmesh/applets/RepeaterAclApplet.cpp
#include <mishmesh/applets/RepeaterAclApplet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/Modal.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

static const char* roleName(uint8_t p) {
  switch (p & 3) { case 1: return "RO"; case 2: return "RW"; case 3: return "Admin"; default: return "-"; }
}
static void hex12(char* out, const uint8_t* pk) {
  static const char* H = "0123456789abcdef";
  for (int i = 0; i < 6; i++) { out[i*2] = H[pk[i] >> 4]; out[i*2+1] = H[pk[i] & 15]; }
  out[12] = 0;
}

const char* RepeaterAclApplet::ListModel_::label(int i) const {
  static char buf[24];
  if (i >= p->_view.count) return "Add user";
  char h[13]; hex12(h, p->_view.entries[i].pubkey);
  snprintf(buf, sizeof(buf), "%s %s", h, roleName(p->_view.entries[i].perms));
  return buf;
}

// Level chooser model: Read-only / Read-write / Admin / Remove.
static const char* const LEVEL_LABELS[] = {"Read-only", "Read-write", "Admin", "Remove"};
static StaticListModel s_levelModel(LEVEL_LABELS, 4);
static const uint8_t LEVEL_PERMS[4] = {1, 2, 3, 0};   // RO, RW, Admin, Remove(=0)

void RepeaterAclApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterAclApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.contacts; _app = ctx.app;
  _model.p = this;
  _menu.setModel(&_model); _menu.resetSelection();
  _levelMenu.setModel(&s_levelModel);
  _status[0] = 0; _pickReady = false;
  beginFetch();
}

void RepeaterAclApplet::onForeground() {
  if (_phase == Phase::Picking) {
    if (_pickReady) { _pickReady = false; _levelMenu.resetSelection(); _phase = Phase::Level; }
    else _phase = Phase::List;   // pick cancelled
  }
}

void RepeaterAclApplet::beginFetch() {
  _op = Op::Fetch; _phase = Phase::Loading;
  _startSeq = _svc ? _svc->accessListSeq() : 0;
  _req.timeoutMs = 20000; _req.begin(_startSeq, _host ? _host->nowMs() : 0);
  if (_svc) _svc->requestAccessList(_pub);
  if (_host) _host->requestRender();
}

void RepeaterAclApplet::fireSetperm(uint8_t perms) {
  char h[13]; hex12(h, _target);
  char cmd[40]; snprintf(cmd, sizeof(cmd), "setperm %s %u", h, (unsigned)perms);
  _op = Op::Setperm; _phase = Phase::Busy;
  _startSeq = _svc ? _svc->cliSeq() : 0;
  _req.timeoutMs = 20000; _req.begin(_startSeq, _host ? _host->nowMs() : 0);
  if (_svc) _svc->sendCliCommand(_pub, cmd);
  if (_host) _host->requestRender();
}

void RepeaterAclApplet::onPicked(void* ctx, const uint8_t* pubKey) {
  auto* self = static_cast<RepeaterAclApplet*>(ctx);
  memcpy(self->_target, pubKey, 6);
  self->_pickReady = true;
}

int RepeaterAclApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  const Font* cap = fontCaption();
  int caph = c.lineHeight(cap);

  if ((_phase == Phase::Loading || _phase == Phase::Busy) && _svc) {
    uint32_t seq = (_op == Op::Fetch) ? _svc->accessListSeq() : _svc->cliSeq();
    PendingRequest::State st = _req.poll(seq, c.now());
    if (st == PendingRequest::State::Ready) {
      if (_op == Op::Fetch) {
        if (_svc->latestAccessList(_pub, _view)) { _op = Op::None; _phase = Phase::List; }
        else _req.rearm(seq);
      } else {   // Setperm
        bool ok = false; const char* resp = nullptr;
        if (_svc->cliResult(_pub, _startSeq, ok, resp)) {
          if (resp && resp[0] == 'O' && resp[1] == 'K') beginFetch();   // re-list
          else { copyStr(_status, sizeof(_status), resp ? resp : "err"); _op = Op::None; _phase = Phase::List; }
        } else _req.rearm(seq);
      }
    } else if (st == PendingRequest::State::TimedOut) {
      strncpy(_status, "[no response]", sizeof(_status) - 1); _op = Op::None; _phase = Phase::List;
    }
  }

  int bh = drawTopBar(c, _bar, _name[0] ? _name : "Access", _app, w);
  int top = bh + 1, bottom = h - caph - 1;
  if (_phase == Phase::Loading) {
    c.drawTextCentered(fontBody(), 0, top, w, bottom - top, "Loading...", DisplayDriver::LIGHT);
  } else {
    _menu.draw(c, 0, top, w, bottom - top);
    const char* status = _phase == Phase::Busy ? "Working..." : (_view.count == 0 ? "No users" : _status);
    c.drawText(cap, 2, bottom, status, DisplayDriver::LIGHT, TextAlign::Left);
  }
  if (_phase == Phase::Level) {
    Canvas box = drawModalChrome(c);
    _levelMenu.draw(box, 2, 2, box.width() - 4, box.height() - 4);
  }

  if (_phase == Phase::Loading || _phase == Phase::Busy) return 150;
  return (_menu.needsAnimation() || _levelMenu.needsAnimation()) ? ListMenu::TICK_MS : 1000;
}

bool RepeaterAclApplet::onInput(InputEvent ev) {
  if (_phase == Phase::Level) {
    if (_levelMenu.onInput(ev)) return true;
    if (ev == InputEvent::Select) { fireSetperm(LEVEL_PERMS[_levelMenu.selected()]); return true; }
    if (ev == InputEvent::Back) { _phase = Phase::List; return true; }
    return true;   // modal
  }
  if (_menu.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    _status[0] = 0;
    int sel = _menu.selected();
    if (sel < _view.count) {                       // edit an entry
      memcpy(_target, _view.entries[sel].pubkey, 6);
      _levelMenu.resetSelection(); _phase = Phase::Level;
    } else {                                        // Add user -> contact picker
      contactsApplet().configurePick(&RepeaterAclApplet::onPicked, this);
      _phase = Phase::Picking;
      if (_host) _host->push(&contactsApplet());
    }
    return true;
  }
  return false;   // Back bubbles
}

RepeaterAclApplet& repeaterAclApplet() {
  static RepeaterAclApplet s_acl;
  return s_acl;
}

}  // namespace mishmesh
