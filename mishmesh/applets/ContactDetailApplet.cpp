#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/applets/TelemetryApplet.h>
#include <mishmesh/applets/PingApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

// telemetryApplet()/pingApplet() (+ their setTarget helpers) come from their headers.

static const char* ACTION_LABELS[ContactDetailApplet::ACTION_KINDS] = {
  "View details", "Telemetry", "Ping", "Reset path", "Clear conversation", "Delete contact",
};

static const char* typeName(uint8_t t) {
  switch (t) {
    case (uint8_t)ContactKind::Chat:     return "User";
    case (uint8_t)ContactKind::Repeater: return "Repeater";
    case (uint8_t)ContactKind::Room:     return "Room";
    case (uint8_t)ContactKind::Sensor:   return "Sensor";
    default:                             return "Contact";
  }
}

ContactDetailApplet::ContactDetailApplet()
    : Applet("Contact"), _host(nullptr), _svc(nullptr), _app(nullptr), _type(0),
      _hasPath(false), _favourite(false), _lastAdvert(0), _actionCount(0),
      _pendingToast(nullptr), _confirming(false), _viewing(false), _pendingAction(-1) {
  _pubkey[0] = 0; _name[0] = 0;
}

const char* ContactDetailApplet::label(int i) const {
  if (i < 0 || i >= _actionCount) return "";
  return ACTION_LABELS[_actions[i]];
}

void ContactDetailApplet::setTarget(const uint8_t* pubKey) {
  memcpy(_pubkey, pubKey, 6);
}

void ContactDetailApplet::buildActions() {
  _actionCount = 0;
  _actions[_actionCount++] = View;
  _actions[_actionCount++] = Telemetry;
  if (_type == (uint8_t)ContactKind::Repeater) _actions[_actionCount++] = Ping;
  _actions[_actionCount++] = ResetPath;
  // Clear conversation only makes sense for conversational contacts.
  if (_type == (uint8_t)ContactKind::Chat || _type == (uint8_t)ContactKind::Room)
    _actions[_actionCount++] = ClearConvo;
  _actions[_actionCount++] = Delete;
}

void ContactDetailApplet::refresh() {
  _name[0] = 0; _type = 0; _hasPath = false; _favourite = false; _lastAdvert = 0;
  if (_svc) {
    for (int k = 1; k <= 4; k++) {
      ContactKind kind = (ContactKind)k;
      int n = _svc->countByKind(kind);
      for (int i = 0; i < n; i++) {
        ContactView v;
        if (_svc->getByKind(kind, i, v) && memcmp(v.pubKey, _pubkey, 6) == 0) {
          strncpy(_name, v.name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = 0;
          _type = v.type; _hasPath = v.hasPath; _favourite = v.isFavourite;
          _lastAdvert = v.lastAdvert;
          buildActions();
          return;
        }
      }
    }
  }
  buildActions();
}

void ContactDetailApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _app = ctx.app;
  refresh();
  _header.set(_name, fontTitle());
  _list.setRowHeight(12);
  _list.setModel(this);
  _confirming = false; _viewing = false; _pendingAction = -1; _pendingToast = nullptr;
}

void ContactDetailApplet::drawDetails(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  int lh = view.lineHeight(fontBody());
  int ry = 0;
  char line[40];

  snprintf(line, sizeof(line), "Type: %s", typeName(_type));
  view.drawText(fontBody(), 2, ry, line, DisplayDriver::LIGHT); ry += lh;

  snprintf(line, sizeof(line), "Key: %02X%02X%02X%02X%02X%02X",
           _pubkey[0], _pubkey[1], _pubkey[2], _pubkey[3], _pubkey[4], _pubkey[5]);
  view.drawText(fontBody(), 2, ry, line, DisplayDriver::LIGHT); ry += lh;

  snprintf(line, sizeof(line), "Path: %s", _hasPath ? "Direct" : "Flood");
  view.drawText(fontBody(), 2, ry, line, DisplayDriver::LIGHT); ry += lh;

  if (_app) {
    uint32_t now = _app->epochSeconds();
    if (now != 0 && _lastAdvert != 0 && now >= _lastAdvert) {
      uint32_t s = now - _lastAdvert;
      if (s < 60)        snprintf(line, sizeof(line), "Heard: %us ago", s);
      else if (s < 3600) snprintf(line, sizeof(line), "Heard: %um ago", s / 60);
      else if (s < 86400)snprintf(line, sizeof(line), "Heard: %uh ago", s / 3600);
      else               snprintf(line, sizeof(line), "Heard: %ud ago", s / 86400);
      view.drawText(fontBody(), 2, ry, line, DisplayDriver::LIGHT); ry += lh;
    }
  }
  if (_favourite) view.drawText(fontBody(), 2, ry, "Favourite", DisplayDriver::LIGHT);
}

int ContactDetailApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  uint32_t now = c.now();
  int hh = c.fontHeight(fontTitle()) + 2;
  _header.set(_name, fontTitle());
  _header.draw(c, 0, 0, w, hh);
  c.fillRect(0, hh, w, 1, DisplayDriver::LIGHT);

  if (_viewing) drawDetails(c, 0, hh + 2, w, h - hh - 2);
  else          _list.draw(c, 0, hh + 2, w, h - hh - 2);

  if (_confirming) _confirm.draw(c, 0, 0, w, h);

  if (_pendingToast) { _toast.show(_pendingToast, now); _pendingToast = nullptr; }
  bool toasting = _toast.active(now);
  if (toasting) _toast.draw(c, 0, h - 14, w, 14);

  if (_confirming) return 100;
  if (toasting) return 150;
  return (!_viewing && _list.needsAnimation()) ? 90 : 500;
}

bool ContactDetailApplet::onInput(InputEvent ev) {
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _svc) {
          if (_pendingAction == ClearConvo) { _svc->clearConversation(_pubkey); _pendingToast = "Cleared"; }
          else if (_pendingAction == Delete) { _svc->deleteContact(_pubkey); _confirming = false; _pendingAction = -1; if (_host) _host->pop(); return true; }
        }
        _confirming = false; _pendingAction = -1;
      }
      return true;
    }
    return true;
  }

  if (_viewing) {                 // details panel: Back returns to the action list
    if (ev == InputEvent::Back) { _viewing = false; return true; }
    return true;                  // consume nav while viewing
  }

  if (_list.onInput(ev)) return true;

  if (ev == InputEvent::Select && _svc) {
    int action = _actions[_list.selected()];
    switch (action) {
      case View: _viewing = true; return true;
      case Telemetry:
        telemetrySetTarget(_pubkey);
        if (_host) _host->push(&telemetryApplet());
        return true;
      case Ping:
        pingSetTarget(_pubkey);
        if (_host) _host->push(&pingApplet());
        return true;
      case ResetPath: _svc->resetPath(_pubkey); _pendingToast = "Path reset"; return true;
      case ClearConvo:
        _pendingAction = ClearConvo;
        _confirm.configure("Clear conversation?");
        _confirming = true; return true;
      case Delete:
        _pendingAction = Delete;
        _confirm.configure("Delete this contact?");
        _confirming = true; return true;
    }
  }
  return false;   // Back -> host pops to list
}

ContactDetailApplet& contactDetailApplet() {
  static ContactDetailApplet s_detail;
  return s_detail;
}

}  // namespace mishmesh
