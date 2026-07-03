#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/applets/ContactPermissionsApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/applets/ServerLoginApplet.h>
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
  "Send message", "Rename", "Permissions", "Set path", "Manage",
};

static const char* typeName(uint8_t t) { return contactTypeName(t); }
static void formatAge(uint32_t s, char* b, int n) { contactFormatAge(s, b, n); }

static int hexNibble(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

// "1-byte (63 hops)" etc. 6-bit hop field caps the count at 63 regardless of size.
static void hashSizeLabel(int v, char* out, uint16_t cap) {
  int maxHops = PATH_MAX_BYTES / v;
  if (maxHops > 63) maxHops = 63;
  snprintf(out, cap, "%d-byte (%d hops)", v, maxHops);
}

// Count comma/space separated tokens (no validation) for the field summary.
static int countHops(const char* text) {
  int hops = 0; const char* p = text;
  while (*p) {
    while (*p == ',' || *p == ' ') p++;
    if (!*p) break;
    hops++;
    while (*p && *p != ',' && *p != ' ') p++;
  }
  return hops;
}

// Text field display: raw hex -> "flood" / "N hop(s)" (keypad still edits the hex).
static void pathSummary(const char* buf, char* out, uint16_t cap) {
  int n = countHops(buf);
  if (n == 0) { snprintf(out, cap, "flood"); return; }
  snprintf(out, cap, "%d hop%s", n, n == 1 ? "" : "s");
}

// Path field accepts empty (= flood); the real hex check happens in submitSetPath.
static bool acceptAny(const char*) { return true; }

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
  // Repeaters get Manage first (primary), like Message is primary for users/rooms.
  if (_type == (uint8_t)ContactKind::Repeater) _actions[_actionCount++] = Manage;
  // Rooms get Message too (routed through login); primary for users and rooms.
  if (_type == (uint8_t)ContactKind::Chat || _type == (uint8_t)ContactKind::Room)
    _actions[_actionCount++] = Message;
  _actions[_actionCount++] = View;
  _actions[_actionCount++] = Rename;     // local display label, all contact types
  _actions[_actionCount++] = Favourite;
  _actions[_actionCount++] = Telemetry;
  _actions[_actionCount++] = Permissions;   // who may pull telemetry from us
  if (_type == (uint8_t)ContactKind::Repeater) _actions[_actionCount++] = Ping;
  _actions[_actionCount++] = SetPath;       // manual routing path
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

int ContactDetailApplet::parseHexPath(const char* text, uint8_t hashSize, uint8_t* out, int outCap) {
  if (hashSize < 1 || hashSize > 3) return -1;
  int bytes = 0, hops = 0;
  const char* p = text;
  while (*p) {
    while (*p == ',' || *p == ' ') p++;        // skip separators / padding
    if (!*p) break;
    for (int b = 0; b < hashSize; b++) {        // exactly hashSize bytes per hop
      int hi = hexNibble(p[0]);
      int lo = (hi < 0) ? -1 : hexNibble(p[1]);
      if (hi < 0 || lo < 0) return -1;
      if (bytes >= outCap) return -1;
      out[bytes++] = (uint8_t)((hi << 4) | lo);
      p += 2;
    }
    if (*p && *p != ',' && *p != ' ') return -1;  // token longer than hashSize bytes
    if (++hops > 63) return -1;
  }
  return hops;
}

void ContactDetailApplet::openSetPath() {
  _hashSize = 1;
  _pathBuf[0] = 0;
  // Pre-fill with the contact's current path, if any.
  uint8_t path[PATH_MAX_BYTES]; uint8_t encoded = 0;
  if (_svc && _svc->getPath(_pubkey, path, encoded)) {
    _hashSize = (int)((encoded >> 6) + 1);
    uint8_t hops = encoded & 63;
    int o = 0;
    for (int h = 0; h < hops; h++) {
      if (h && o < (int)sizeof(_pathBuf) - 1) _pathBuf[o++] = ',';
      for (int b = 0; b < _hashSize && o < (int)sizeof(_pathBuf) - 3; b++)
        o += snprintf(_pathBuf + o, sizeof(_pathBuf) - o, "%02x", path[h * _hashSize + b]);
    }
    _pathBuf[o] = 0;
  }
  FormApplet::Field f[2] = {
    { "Hash size", nullptr, 0, nullptr, nullptr, nullptr,
      FormApplet::Stepper, &_hashSize, 1, 3, hashSizeLabel },
    { "Path", _pathBuf, sizeof(_pathBuf), acceptAny, nullptr, pathSummary },
  };
  formApplet().configure(_displayName, f, 2, &ContactDetailApplet::submitSetPath, this, "Save");
  if (_host) _host->push(&formApplet());
}

bool ContactDetailApplet::submitSetPath(void* ctx) {
  ContactDetailApplet* a = (ContactDetailApplet*)ctx;
  if (!a->_svc) return true;
  uint8_t path[PATH_MAX_BYTES];
  int hops = ContactDetailApplet::parseHexPath(a->_pathBuf, (uint8_t)a->_hashSize, path, PATH_MAX_BYTES);
  if (hops < 0) { if (a->_host) a->_host->postToast("Invalid path"); return false; }
  uint8_t encoded = (uint8_t)(((a->_hashSize - 1) << 6) | (hops & 63));
  if (a->_svc->setPath(a->_pubkey, path, encoded)) {
    if (a->_host) a->_host->postToast(hops ? "Path saved" : "Path cleared");
    return true;
  }
  if (a->_host) a->_host->postToast("Failed");
  return false;
}

void ContactDetailApplet::setSetPathTextForTest(const char* s, uint8_t hashSize) {
  strncpy(_pathBuf, s ? s : "", sizeof(_pathBuf) - 1);
  _pathBuf[sizeof(_pathBuf) - 1] = 0;
  _hashSize = hashSize;
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
      case Manage:
        serverLoginApplet().setTarget(_pubkey, _name, ServerLoginApplet::Mode::Repeater);
        if (_host) _host->push(&serverLoginApplet());
        return true;
      case Message:
        // Rooms require a server login before posting; go through ServerLoginApplet
        // unless we already logged in this power cycle (then straight to the thread).
        if (_type == (uint8_t)ContactKind::Room && !_svc->isLoggedIn(_pubkey)) {
          serverLoginApplet().setTarget(_pubkey, _name, ServerLoginApplet::Mode::Room);
          if (_host) _host->push(&serverLoginApplet());
          return true;
        }
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
      case Permissions:
        contactPermissionsApplet().setTarget(_pubkey, _displayName);
        if (_host) _host->push(&contactPermissionsApplet());
        return true;
      case SetPath:
        openSetPath();
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
