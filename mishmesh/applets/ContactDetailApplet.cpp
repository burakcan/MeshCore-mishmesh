#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/ContactFormat.h>
#include <mishmesh/core/Geo.h>
#include <mishmesh/core/MessageStore.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const uint32_t PING_TIMEOUT_MS = 12000;
static const uint32_t TELEM_TIMEOUT_MS = 12000;

// Favourite's label is dynamic (see label()); this slot is a placeholder.
static const char* ACTION_LABELS[ContactDetailApplet::ACTION_KINDS] = {
  "View details", "", "Telemetry", "Ping (0 hop)", "Reset path", "Clear conversation", "Delete contact",
  "Send message", "Rename",
};

static const char* typeName(uint8_t t) { return contactTypeName(t); }
static void formatAge(uint32_t s, char* b, int n) { contactFormatAge(s, b, n); }

ContactDetailApplet::ContactDetailApplet()
    : Applet("Contact"), _host(nullptr), _svc(nullptr), _app(nullptr), _type(0),
      _hasPath(false), _hops(0), _favourite(false), _lastAdvert(0),
      _hasLoc(false), _gpsLat(0), _gpsLon(0), _distKm(-1), _actionCount(0),
      _confirming(false), _viewing(false), _pinging(false), _pingDone(false),
      _pingStartSeq(0), _pingStartMs(0), _telemActive(false), _telemDone(false),
      _telemStartSeq(0), _telemStartMs(0), _pendingAction(-1) {
  _pubkey[0] = 0; _name[0] = 0; _displayName[0] = 0; _infoLine[0] = 0;
}

const char* ContactDetailApplet::label(int i) const {
  if (i < 0 || i >= _actionCount) return "";
  if (_actions[i] == Favourite) return _favourite ? "Remove from favorites" : "Add to favorites";
  return ACTION_LABELS[_actions[i]];
}

void ContactDetailApplet::setTarget(const uint8_t* pubKey) {
  memcpy(_pubkey, pubKey, 6);
}

void ContactDetailApplet::buildActions() {
  _actionCount = 0;
  if (_type == (uint8_t)ContactKind::Chat) _actions[_actionCount++] = Message;   // primary for users
  _actions[_actionCount++] = View;
  _actions[_actionCount++] = Rename;     // local display label, all contact types
  _actions[_actionCount++] = Favourite;
  _actions[_actionCount++] = Telemetry;
  if (_type == (uint8_t)ContactKind::Repeater) _actions[_actionCount++] = Ping;
  _actions[_actionCount++] = ResetPath;
  // Clear conversation only makes sense for conversational contacts.
  if (_type == (uint8_t)ContactKind::Chat || _type == (uint8_t)ContactKind::Room)
    _actions[_actionCount++] = ClearConvo;
  _actions[_actionCount++] = Delete;
}

// Keypad confirm (ctx = this applet): rename the contact, then refresh the card.
// Runs before the keypad pops, so the detail screen re-renders with the new name.
void ContactDetailApplet::onRenameDone(void* ctx, const char* text) {
  auto* self = static_cast<ContactDetailApplet*>(ctx);
  if (!text || !text[0] || !self->_svc) return;   // empty -> keep the old name
  if (self->_svc->renameContact(self->_pubkey, text)) self->refresh();
}

void ContactDetailApplet::refresh() {
  _name[0] = 0; _type = 0; _hasPath = false; _hops = 0; _favourite = false;
  _lastAdvert = 0; _hasLoc = false; _gpsLat = _gpsLon = 0; _distKm = -1;
  memset(_fullKey, 0, sizeof(_fullKey));
  if (_svc) {
    for (int k = 1; k <= 4; k++) {
      ContactKind kind = (ContactKind)k;
      int n = _svc->countByKind(kind);
      for (int i = 0; i < n; i++) {
        ContactView v;
        if (_svc->getByKind(kind, i, v) && memcmp(v.pubKey, _pubkey, 6) == 0) {
          strncpy(_name, v.name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = 0;
          _type = v.type; _hasPath = v.hasPath; _hops = v.hops;
          _favourite = v.isFavourite; _lastAdvert = v.lastAdvert;
          memcpy(_fullKey, v.pubKey, PUBKEY_LEN);
          _hasLoc = v.hasLocation; _gpsLat = v.gpsLat; _gpsLon = v.gpsLon;
          int32_t slat, slon;
          if (_hasLoc && _svc->selfLocation(slat, slon))
            _distKm = geoDistanceKm(slat, slon, _gpsLat, _gpsLon);
          break;
        }
      }
    }
  }
  buildActions();
  buildInfo();
}

void ContactDetailApplet::buildInfo() {
  // Repeaters: prefix the name with the first 2 hex of the key.
  if (_type == (uint8_t)ContactKind::Repeater)
    snprintf(_displayName, sizeof(_displayName), "%02X %s", _fullKey[0], _name);
  else
    snprintf(_displayName, sizeof(_displayName), "%s", _name);

  char age[12]; age[0] = 0;
  if (_app) {
    uint32_t now = _app->epochSeconds();
    if (now != 0 && _lastAdvert != 0 && now >= _lastAdvert) formatAge(now - _lastAdvert, age, sizeof(age));
  }
  // "Type | 1.2km | 5m"  (Nokia's font lacks the middle-dot glyph)
  int p = snprintf(_infoLine, sizeof(_infoLine), "%s", typeName(_type));
  if (_distKm >= 0) p += snprintf(_infoLine + p, sizeof(_infoLine) - p, " | %.2fkm", (double)_distKm);
  if (age[0])       p += snprintf(_infoLine + p, sizeof(_infoLine) - p, " | %s", age);
  _card.set(_displayName, _infoLine);
  _card.setFavourite(_favourite);   // star on the title bar

  // The view-details panel.
  _details.clear();
  _details.addf("Type: %s", typeName(_type));
  if (_hasPath) _details.addf("Path: Direct (%u hop)", _hops);
  else          _details.addLine("Path: Flood");
  if (_distKm >= 0) _details.addf("Distance: %.2f km", (double)_distKm);
  if (age[0])       _details.addf("Last heard: %s ago", age);
  if (_hasLoc)      _details.addf("GPS: %.5f, %.5f", _gpsLat / 1e6, _gpsLon / 1e6);
  _details.addLine("Public key:");
  for (int r = 0; r < PUBKEY_LEN; r += 8) {     // 8 bytes / 16 hex chars per line
    char hex[20];
    for (int j = 0; j < 8; j++) snprintf(hex + j * 2, 3, "%02X", _fullKey[r + j]);
    _details.addLine(hex);
  }
}

void ContactDetailApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _app = ctx.app;
  refresh();
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();   // shared static applet: always reopen at the first action
  _confirming = false; _viewing = false; _pinging = false; _pingDone = false;
  _telemActive = false; _telemDone = false;
  _pendingAction = -1;
}

int ContactDetailApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int cardH = _card.height(c);

  if (_viewing) {
    _details.setHeader(&_card, cardH);   // card scrolls together with the detail lines
    _details.draw(c, 0, 0, w, h);
  } else {
    _list.setHeader(&_card, cardH);      // card scrolls together with the action list
    _list.draw(c, 0, 0, w, h);
  }

  if (_confirming) { _confirm.draw(c, 0, 0, w, h); return 100; }

  if (_pinging) {
    uint32_t now = c.now();
    if (_pingStartMs == 0) _pingStartMs = now;
    if (!_pingDone && _svc) {
      PingView v;
      if (_svc->pingSeq() != _pingStartSeq && _svc->latestPing(_pubkey, v) && v.replied) {
        _ping.setReplied(v.rttMs, v.snrUs, v.snrThem); _pingDone = true;
      } else if (now - _pingStartMs > PING_TIMEOUT_MS) {
        _ping.setTimeout(); _pingDone = true;
      }
    }
    _ping.draw(c, 0, 0, w, h);
    if (_ping.needsAnimation()) return ListMenu::TICK_MS;
    return _pingDone ? 500 : 200;
  }

  if (_telemActive) {
    uint32_t now = c.now();
    if (_telemStartMs == 0) _telemStartMs = now;
    if (!_telemDone && _svc) {
      TelemetryView v;
      if (_svc->telemetrySeq() != _telemStartSeq && _svc->latestTelemetry(_pubkey, v)) {
        _telem.setResult(v); _telemDone = true;
      } else if (now - _telemStartMs > TELEM_TIMEOUT_MS) {
        _telem.setTimeout(); _telemDone = true;
      }
    }
    _telem.draw(c, 0, 0, w, h);
    if (_telem.needsAnimation()) return ListMenu::TICK_MS;
    return _telemDone ? 500 : 200;
  }
  bool anim = _card.needsAnimation() || (_viewing ? _details.needsAnimation() : _list.needsAnimation());
  return anim ? ListMenu::TICK_MS : 500;
}

bool ContactDetailApplet::onInput(InputEvent ev) {
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _svc) {
          if (_pendingAction == ClearConvo) { _svc->clearConversation(_pubkey); if (_host) _host->postToast("Cleared"); }
          else if (_pendingAction == Delete) {
            _svc->deleteContact(_pubkey);
            _confirming = false; _pendingAction = -1;
            if (_host) { _host->postToast("Contact deleted"); _host->pop(); }
            return true;
          }
        }
        _confirming = false; _pendingAction = -1;
      }
      return true;
    }
    return true;
  }

  if (_pinging) {                 // ping modal: NavUp/Down scroll, others close it
    if (_ping.onInput(ev)) return true;
    if (ev == InputEvent::Back || ev == InputEvent::Select || ev == InputEvent::Cancel)
      _pinging = false;
    return true;                  // swallow input while the modal is up
  }

  if (_telemActive) {             // telemetry modal: same scroll-or-close handling
    if (_telem.onInput(ev)) return true;
    if (ev == InputEvent::Back || ev == InputEvent::Select || ev == InputEvent::Cancel)
      _telemActive = false;
    return true;
  }

  if (_viewing) {                 // details panel: scrollable; Back returns to the list
    if (ev == InputEvent::Back) { _viewing = false; return true; }
    _details.onInput(ev);         // NavUp/Down scroll
    return true;
  }

  if (_list.onInput(ev)) return true;

  if (ev == InputEvent::Select && _svc) {
    int action = _actions[_list.selected()];
    switch (action) {
      case Message:
        messageThreadApplet().setTarget(directKey(_pubkey), _name);
        messageThreadApplet().composeOnOpen();   // intent is to write -> focus the Write button
        if (_host) _host->push(&messageThreadApplet());
        return true;
      case Rename:
        strncpy(_renameBuf, _name, sizeof(_renameBuf) - 1);   // seed with the current name
        _renameBuf[sizeof(_renameBuf) - 1] = 0;
        keypadApplet().configure(_renameBuf, sizeof(_renameBuf) - 1, "Rename",
                                 &ContactDetailApplet::onRenameDone, this);
        if (_host) _host->push(&keypadApplet());
        return true;
      case View: _viewing = true; return true;
      case Favourite:
        if (_svc->setFavourite(_pubkey, !_favourite)) {
          _favourite = !_favourite;
          _card.setFavourite(_favourite);     // label + star update on next render
          if (_host) _host->postToast(_favourite ? "Added to favorites" : "Removed from favorites");
        }
        return true;
      case Telemetry:
        _svc->requestTelemetry(_pubkey);
        _telemStartSeq = _svc->telemetrySeq();
        _telemStartMs = 0; _telemDone = false;
        _telem.setWaiting();
        _telemActive = true;
        return true;
      case Ping:
        _svc->ping(_pubkey);
        _pingStartSeq = _svc->pingSeq();
        _pingStartMs = 0; _pingDone = false;
        _ping.setWaiting();
        _pinging = true;
        return true;
      case ResetPath: _svc->resetPath(_pubkey); if (_host) _host->postToast("Path reset"); return true;
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
