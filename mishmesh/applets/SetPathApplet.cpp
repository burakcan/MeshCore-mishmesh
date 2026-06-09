#include <mishmesh/applets/SetPathApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const char* ROW_LABELS[SetPathApplet::ROW_COUNT] = { "Hash size", "Path", "Save path" };

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

int SetPathApplet::parseHexPath(const char* text, uint8_t hashSize, uint8_t* out, int outCap) {
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

// Count tokens for the row's summary value (no validation; the keypad shows the text).
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

SetPathApplet::SetPathApplet()
    : Applet("Set Path"), _host(nullptr), _svc(nullptr), _favourite(false), _hashSize(1), _editingSize(false) {
  _pubkey[0] = 0; _name[0] = 0; _info[0] = 0; _pathBuf[0] = 0;
}

void SetPathApplet::setTarget(const uint8_t* pubKey, const char* name,
                              const char* info, bool favourite) {
  memcpy(_pubkey, pubKey, 6);
  strncpy(_name, name ? name : "", sizeof(_name) - 1);
  _name[sizeof(_name) - 1] = 0;
  strncpy(_info, info ? info : "", sizeof(_info) - 1);
  _info[sizeof(_info) - 1] = 0;
  _favourite = favourite;
}

const char* SetPathApplet::label(int i) const {
  return (i >= 0 && i < ROW_COUNT) ? ROW_LABELS[i] : "";
}

const char* SetPathApplet::value(int i) const {
  static char buf[24];
  if (i == HashSize) { hashSizeLabel(_hashSize, buf, sizeof(buf)); return buf; }
  if (i == Path) {
    int n = countHops(_pathBuf);
    if (n == 0) { snprintf(buf, sizeof(buf), "flood"); return buf; }
    snprintf(buf, sizeof(buf), "%d hop%s", n, n == 1 ? "" : "s");
    return buf;
  }
  return nullptr;   // Save row has no value
}

void SetPathApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _hashSize = 1;
  _pathBuf[0] = 0;
  // Pre-fill with the contact's current path, if any.
  uint8_t path[PATH_MAX_BYTES]; uint8_t encoded = 0;
  if (_svc && _svc->getPath(_pubkey, path, encoded)) {
    _hashSize = (uint8_t)((encoded >> 6) + 1);
    uint8_t hops = encoded & 63;
    int o = 0;
    for (int h = 0; h < hops; h++) {
      if (h && o < (int)sizeof(_pathBuf) - 1) _pathBuf[o++] = ',';
      for (int b = 0; b < _hashSize && o < (int)sizeof(_pathBuf) - 3; b++)
        o += snprintf(_pathBuf + o, sizeof(_pathBuf) - o, "%02x", path[h * _hashSize + b]);
    }
    _pathBuf[o] = 0;
  }
  _card.set(_name, _info);          // same name+info card as the contact detail page
  _card.setFavourite(_favourite);
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
  _editingSize = false;
}

int SetPathApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  _list.setHeader(&_card, _card.height(c));
  _list.draw(c, 0, 0, w, h);
  if (_editingSize) { _sizeStepper.draw(c, 0, 0, w, h); return 100; }
  bool anim = _card.needsAnimation() || _list.needsAnimation();
  return anim ? ListMenu::TICK_MS : 500;
}

bool SetPathApplet::onInput(InputEvent ev) {
  if (_editingSize) {
    if (_sizeStepper.onInput(ev)) {
      StepperResult r = _sizeStepper.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed) _hashSize = (uint8_t)_sizeStepper.value();
        _editingSize = false;
        _sizeStepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;

  if (ev == InputEvent::Select) {
    switch (_list.selected()) {
      case HashSize:
        _sizeStepper.configure("Hash size", _hashSize, 1, 3, hashSizeLabel);
        _editingSize = true;
        return true;
      case Path:
        keypadApplet().configure(_pathBuf, sizeof(_pathBuf), "Path");   // edits _pathBuf in place
        if (_host) _host->push(&keypadApplet());
        return true;
      case Save: {
        if (!_svc) return true;
        uint8_t path[PATH_MAX_BYTES];
        int hops = parseHexPath(_pathBuf, _hashSize, path, PATH_MAX_BYTES);
        if (hops < 0) { if (_host) _host->postToast("Invalid path"); return true; }
        uint8_t encoded = (uint8_t)(((_hashSize - 1) << 6) | (hops & 63));
        if (_svc->setPath(_pubkey, path, encoded)) {
          if (_host) { _host->postToast(hops ? "Path saved" : "Path cleared"); _host->pop(); }
        } else if (_host) {
          _host->postToast("Failed");
        }
        return true;
      }
    }
  }
  return false;   // Back bubbles -> host pops back to the contact detail
}

void SetPathApplet::setPathTextForTest(const char* s, uint8_t hashSize) {
  strncpy(_pathBuf, s ? s : "", sizeof(_pathBuf) - 1);
  _pathBuf[sizeof(_pathBuf) - 1] = 0;
  _hashSize = hashSize;
}

SetPathApplet& setPathApplet() {
  static SetPathApplet s_setpath;
  return s_setpath;
}

}  // namespace mishmesh
