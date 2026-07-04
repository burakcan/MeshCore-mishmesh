// mishmesh/applets/RepeaterActionsPanel.cpp
#include <mishmesh/applets/RepeaterActionsPanel.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/CliRequest.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

static const char* const ACTIONS_LABELS[] = {"Send advert", "Sync clock", "Reboot"};
static StaticListModel s_actionsModel(ACTIONS_LABELS, 3);

void RepeaterActionsPanel::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterActionsPanel::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _app = ctx.app;
  _confirming = false;
  _phase = Phase::Idle;
  _menu.setModel(&s_actionsModel);
  _menu.resetSelection();
}

void RepeaterActionsPanel::fireAdvert() {
  if (!_svc) return;
  _startSeq = cliFire(_svc, _req, _pub, "advert", _host ? _host->nowMs() : 0, 8000);
  _phase = Phase::Busy;
  if (_host) _host->requestRender();
}

void RepeaterActionsPanel::fireSync() {
  if (!_svc) return;
  uint32_t now = _app ? _app->epochSeconds() : 0;
  char cmd[24];
  snprintf(cmd, sizeof(cmd), "time %lu", (unsigned long)now);
  _startSeq = cliFire(_svc, _req, _pub, cmd, _host ? _host->nowMs() : 0, 8000);
  _phase = Phase::Busy;
  if (_host) _host->requestRender();
}

void RepeaterActionsPanel::fireReboot() {
  if (_svc) _svc->sendCliCommand(_pub, "reboot");   // no reply; session drops
  if (_host) { _host->postToast("Rebooting..."); _host->pop(); }
}

int RepeaterActionsPanel::onRender(Canvas& c) {
  if (_phase == Phase::Busy && _svc) {
    bool ok = false; const char* resp = nullptr;
    CliPoll st = cliPoll(_svc, _req, _pub, _startSeq, c.now(), ok, resp);
    if (st == CliPoll::Ready) {
      if (_host) _host->postToast(resp ? resp : "Done");
      _phase = Phase::Idle;
    } else if (st == CliPoll::TimedOut) {
      if (_host) _host->postToast("[no response]");
      _phase = Phase::Idle;
    }
  }

  const int w = c.width(), h = c.height();
  const Font* cap = fontCaption();
  int caph = c.lineHeight(cap);
  int bh = drawTopBar(c, _bar, _name[0] ? _name : "Actions", _app, w);
  int top = bh + 1, bottom = h - caph - 1;
  _menu.draw(c, 0, top, w, bottom - top);
  const char* status = (_phase == Phase::Busy) ? "Working..." : "";
  if (status[0]) c.drawText(cap, 2, bottom, status, DisplayDriver::LIGHT, TextAlign::Left);
  if (_confirming) _confirm.draw(c, 0, 0, w, h);
  if (_phase == Phase::Busy) return 150;
  return _menu.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool RepeaterActionsPanel::onInput(InputEvent ev) {
  if (_phase == Phase::Busy) return true;   // absorb all input while awaiting reply
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r == ConfirmResult::Confirmed) { _confirming = false; fireReboot(); }
      else if (r == ConfirmResult::Cancelled) { _confirming = false; }
    }
    return true;
  }
  if (_menu.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    switch (_menu.selected()) {
      case ROW_ADVERT: fireAdvert(); return true;
      case ROW_SYNC:   fireSync();   return true;
      default:         _confirm.configure("Reboot repeater?"); _confirming = true; return true;
    }
  }
  return false;   // Back bubbles
}

RepeaterActionsPanel& repeaterActionsPanel() {
  static RepeaterActionsPanel s_actions;
  return s_actions;
}

}  // namespace mishmesh
