// mishmesh/applets/RepeaterRegionsApplet.cpp
#include <mishmesh/applets/RepeaterRegionsApplet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/CliRequest.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

const char* RepeaterRegionsApplet::Model::label(int i) const {
  if (i < p->_count) return p->_reg[i].name;
  int a = i - p->_count;
  return a == 0 ? "Add region" : a == 1 ? "Delete region" : "Save";
}

void RepeaterRegionsApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterRegionsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.contacts; _app = ctx.app;
  _model.p = this;
  _menu.setModel(&_model);
  _menu.resetSelection();
  _status[0] = 0;
  beginFetch();
}

void RepeaterRegionsApplet::onForeground() {
  if (_phase == Phase::Editing) _phase = Phase::List;   // keypad cancelled (onKeypadDone sets Busy)
}

void RepeaterRegionsApplet::fireCmd(Op op, const char* cmd, Phase ph) {
  _op = op; _phase = ph;
  _startSeq = cliFire(_svc, _req, _pub, cmd, _host ? _host->nowMs() : 0, 20000);
  if (_host) _host->requestRender();
}

void RepeaterRegionsApplet::beginFetch() {
  _count = 0;
  fireCmd(Op::FetchAllowed, "region list allowed", Phase::Loading);
}

void RepeaterRegionsApplet::parseList(const char* resp, bool allowed) {
  if (!resp || !resp[0] || strcmp(resp, "-none-") == 0) return;
  const char* s = resp;
  while (*s && _count < MAX_REGIONS) {
    const char* comma = strchr(s, ',');
    int len = comma ? (int)(comma - s) : (int)strlen(s);
    if (len > 0) {
      if (len > NAME_CAP - 1) len = NAME_CAP - 1;
      memcpy(_reg[_count].name, s, len); _reg[_count].name[len] = 0;
      _reg[_count].allowed = allowed; _count++;
    }
    if (!comma) break;
    s = comma + 1;
  }
}

void RepeaterRegionsApplet::onKeypadDone(void* ctx, const char* text) {
  auto* self = static_cast<RepeaterRegionsApplet*>(ctx);
  if (text && text != self->_scratch) {
    strncpy(self->_scratch, text, NAME_CAP - 1); self->_scratch[NAME_CAP - 1] = 0;
  }
  if (!self->_scratch[0]) return;
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "region %s %s", self->_kpDelete ? "remove" : "put", self->_scratch);
  self->fireCmd(self->_kpDelete ? Op::Delete : Op::Add, cmd, Phase::Busy);
}

int RepeaterRegionsApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  const Font* cap = fontCaption();
  int caph = c.lineHeight(cap);

  if ((_phase == Phase::Loading || _phase == Phase::Busy) && _svc) {
    bool ok = false; const char* resp = nullptr;
    CliPoll st = cliPoll(_svc, _req, _pub, _startSeq, c.now(), ok, resp);
    if (st == CliPoll::Ready) {
      {
        const char* r = resp ? resp : "";
        bool okReply = (r[0] == 'O' && r[1] == 'K');
        switch (_op) {
          case Op::FetchAllowed: parseList(r, true);  fireCmd(Op::FetchDenied, "region list denied", Phase::Loading); break;
          case Op::FetchDenied:  parseList(r, false); _op = Op::None; _phase = Phase::List; break;
          case Op::Toggle:
            if (okReply && _toggleIdx >= 0 && _toggleIdx < _count) _reg[_toggleIdx].allowed = !_reg[_toggleIdx].allowed;
            else if (!okReply) { copyStr(_status, sizeof(_status), r); }
            _op = Op::None; _phase = Phase::List; break;
          case Op::Add: case Op::Delete:
            if (okReply) { beginFetch(); }
            else { copyStr(_status, sizeof(_status), r); _op = Op::None; _phase = Phase::List; }
            break;
          case Op::Save:
            if (_host) _host->postToast(okReply ? "Saved" : "Save failed");
            _op = Op::None; _phase = Phase::List; break;
          default: _op = Op::None; _phase = Phase::List; break;
        }
      }
    } else if (st == CliPoll::TimedOut) {
      strncpy(_status, "[no response]", sizeof(_status) - 1);
      _op = Op::None; _phase = Phase::List;
    }
  }

  int bh = drawTopBar(c, _bar, _name[0] ? _name : "Regions", _app, w);
  int top = bh + 1, bottom = h - caph - 1;
  if (_phase == Phase::Loading) {
    c.drawTextCentered(fontBody(), 0, top, w, bottom - top, "Loading...", DisplayDriver::LIGHT);
  } else {
    _menu.draw(c, 0, top, w, bottom - top);
    const char* status = _phase == Phase::Busy ? "Working..." : _status;
    c.drawText(cap, 2, bottom, status, DisplayDriver::LIGHT, TextAlign::Left);
  }

  if (_phase == Phase::Loading || _phase == Phase::Busy) return 150;
  return _menu.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool RepeaterRegionsApplet::onInput(InputEvent ev) {
  if (_menu.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    _status[0] = 0;
    int sel = _menu.selected();
    if (sel < _count) {
      _toggleIdx = sel;
      char cmd[48];
      snprintf(cmd, sizeof(cmd), "region %s %s", _reg[sel].allowed ? "denyf" : "allowf", _reg[sel].name);
      fireCmd(Op::Toggle, cmd, Phase::Busy);
    } else {
      int a = sel - _count;
      if (a == 2) { fireCmd(Op::Save, "region save", Phase::Busy); return true; }
      _kpDelete = (a == 1);
      _scratch[0] = 0;
      _phase = Phase::Editing;
      keypadApplet().configure(_scratch, NAME_CAP - 1, _kpDelete ? "Delete region" : "Region name",
                               &RepeaterRegionsApplet::onKeypadDone, this);
      if (_host) _host->push(&keypadApplet());
    }
    return true;
  }
  return false;   // Back bubbles
}

RepeaterRegionsApplet& repeaterRegionsApplet() {
  static RepeaterRegionsApplet s_regions;
  return s_regions;
}

}  // namespace mishmesh
