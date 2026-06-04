#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

KeypadApplet::KeypadApplet()
    : Applet("Keypad"), _ctx(nullptr), _buf(_own), _cap(KP_MAX), _title("Text"),
      _len(0), _cursor(0), _mode(Mode::Lower), _symPage(false),
      _pending(false), _pendingPos(0), _tapGroupCell(-1), _tapIndex(0),
      _lastTapMs(0), _tapStampPending(false), _now(0),
      _onConfirm(nullptr), _onConfirmCtx(nullptr) {
  _own[0] = 0;
}

const char* KeypadApplet::groupAt(int i) const {
  static const char* const LOWER[9] = {".,?!","abc","def","ghi","jkl","mno","pqrs","tuv","wxyz"};
  static const char* const UPPER[9] = {".,?!","ABC","DEF","GHI","JKL","MNO","PQRS","TUV","WXYZ"};
  static const char* const NUM[9]   = {"1","2","3","4","5","6","7","8","9"};
  static const char* const SYM[9]   = {".,?!",":;\"'","()[]","<>{}","+-*/","=_~","@#&","%$|","^`\\"};
  if (i < 0 || i > 8) return "";
  if (_symPage) return SYM[i];
  if (_mode == Mode::Num) return NUM[i];
  if (_mode == Mode::Upper) return UPPER[i];
  return LOWER[i];
}

const char* KeypadApplet::cellLabel(int r, int c) const {
  if (r < 3 && c < 3) return groupAt(r * 3 + c);
  if (c == 3) {
    if (r == 0) return "DEL";
    if (r == 1) return "SPC";
    if (r == 2) {  // shift cell shows current mode
      return _mode == Mode::Upper ? "AB" : _mode == Mode::Num ? "12" : "ab";
    }
  }
  // r == 3
  if (c == 0) {
    if (_symPage) return "abc";
    if (_mode == Mode::Num) return "0";
    return "sym";
  }
  if (c == 1) return "<";   // cursor left  (icon fallback; Task 5 adds glyph)
  if (c == 2) return ">";   // cursor right
  return "OK";              // c == 3 confirm
}

uint16_t KeypadApplet::cellIcon(int r, int c) const {
  if (r < 3 && c < 3) return 0;                              // char groups: text
  if (c == 3 && r == 0) return (uint16_t)Icon::Backspace;   // DEL
  if (r == 3 && c == 1) return (uint16_t)Icon::ArrowLeft;   // cursor left (existing icon)
  if (r == 3 && c == 2) return (uint16_t)Icon::ArrowRight;  // cursor right
  if (r == 3 && c == 3) return (uint16_t)Icon::Check;       // OK
  return 0;                                                  // SPC / shift / sym: text
}

void KeypadApplet::commitPending() {
  _pending = false;
  _tapGroupCell = -1;
}

void KeypadApplet::cycleMode() {
  commitPending();
  _symPage = false;
  _mode = _mode == Mode::Lower ? Mode::Upper
        : _mode == Mode::Upper ? Mode::Num : Mode::Lower;
}

void KeypadApplet::toggleSymPage() {
  commitPending();
  _symPage = !_symPage;
  if (_symPage) _mode = Mode::Lower;   // symbols are case-independent; normalize
}

void KeypadApplet::insertCharAt(uint16_t pos, char ch) {
  if (_len >= _cap) return;
  for (uint16_t i = _len; i > pos; i--) _buf[i] = _buf[i - 1];
  _buf[pos] = ch;
  _len++;
  _buf[_len] = 0;
}

void KeypadApplet::deleteCharAt(uint16_t pos) {
  if (pos >= _len) return;
  for (uint16_t i = pos; i + 1 < _len; i++) _buf[i] = _buf[i + 1];
  _len--;
  _buf[_len] = 0;
}

void KeypadApplet::handleCharCell(int idx) {
  const char* g = groupAt(idx);
  size_t glen = strlen(g);
  if (glen == 0) return;
  if (_pending && _tapGroupCell == idx) {           // cycle within same group
    _tapIndex = (uint8_t)((_tapIndex + 1) % glen);
    _buf[_pendingPos] = g[_tapIndex];
    _tapStampPending = true;          // restart the timer on the next render (fresh clock)
    return;
  }
  commitPending();
  if (_len >= _cap) return;                          // buffer full
  insertCharAt(_cursor, g[0]);
  _pendingPos = _cursor;
  _cursor++;
  if (glen > 1) {                                    // single-char groups don't cycle
    _pending = true;
    _tapGroupCell = idx;
    _tapIndex = 0;
    _tapStampPending = true;          // stamp the timer on the next render (fresh clock)
  }
}

void KeypadApplet::handleSelect() {
  int r = _grid.focusedRow(), c = _grid.focusedCol();
  if (r < 3 && c < 3) { handleCharCell(r * 3 + c); return; }
  if (c == 3) {
    if (r == 0) { commitPending(); if (_cursor > 0) { deleteCharAt(_cursor - 1); _cursor--; } return; }
    if (r == 1) { commitPending(); insertCharAt(_cursor, ' '); _cursor++; return; }
    if (r == 2) { cycleMode(); return; }            // shift / mode cycle
  }
  // r == 3
  if (c == 0) {
    if (_symPage)            { toggleSymPage(); return; }
    if (_mode == Mode::Num)  { commitPending(); insertCharAt(_cursor, '0'); _cursor++; return; }
    toggleSymPage(); return;
  }
  if (c == 1) { commitPending(); if (_cursor > 0)    _cursor--; return; }   // cursor left
  if (c == 2) { commitPending(); if (_cursor < _len) _cursor++; return; }   // cursor right
  confirmAndExit();                                  // c == 3, OK
}

void KeypadApplet::onStart(AppletContext& ctx) {
  _ctx = &ctx;
  _grid.setModel(this);
  _grid.setFocus(0, 1);            // start on "abc" - the natural first target for typing
  if (_buf == _own) { _own[0] = 0; _len = 0; _cursor = 0; }  // standalone reset
  _mode = Mode::Lower;
  _symPage = false;
  commitPending();
  _tapStampPending = false;
  _now = 0;
}

int KeypadApplet::onRender(Canvas& c) {
  _now = c.now();
  // The multi-tap timer must run off real time, but onInput() has no clock - it
  // only sees the _now cached by the previous render, which after an idle frame
  // can be ~1s stale. So a tap defers its timestamp here, to the first render
  // after it, where _now is fresh; only later renders test the timeout. (Stamping
  // at tap time off the stale clock made the first render commit instantly,
  // turning a quick second tap into a new letter instead of a cycle.)
  if (_tapStampPending) {
    _lastTapMs = _now;
    _tapStampPending = false;
  } else if (_pending && (_now - _lastTapMs) >= TAP_TIMEOUT_MS) {
    commitPending();
  }

  const int tagW = 20;
  const int topH = 13;
  drawBuffer(c, 0, 0, c.width() - tagW, topH);

  const char* tag = _symPage ? "sym"
                  : _mode == Mode::Upper ? "ABC"
                  : _mode == Mode::Num ? "123" : "abc";
  c.drawText(fontCaption(), c.width(), 3, tag, DisplayDriver::LIGHT, TextAlign::Right);

  _grid.draw(c, 0, topH, c.width(), c.height() - topH);
  return _pending ? 80 : 1000;
}

bool KeypadApplet::onInput(InputEvent ev) {
  switch (ev) {
    case InputEvent::NavUp:
    case InputEvent::NavDown:
    case InputEvent::NavLeft:
    case InputEvent::NavRight:
      commitPending();
      return _grid.onInput(ev);
    case InputEvent::Select:
      handleSelect();
      return true;
    case InputEvent::Back:
      commitPending();
      if (_cursor > 0) { deleteCharAt(_cursor - 1); _cursor--; return true; }
      return _len != 0;            // empty -> false (host pops); non-empty at col 0 -> no-op
    case InputEvent::BackLong:
      return false;                // host pops (exit)
    default:
      return false;
  }
}

void KeypadApplet::confirmAndExit() {
  commitPending();
  if (_onConfirm) _onConfirm(_onConfirmCtx, _buf);
  if (_ctx && _ctx->host) {
    if (!_onConfirm) _ctx->host->postToast(_len ? "Saved" : "(empty)");
    _ctx->host->pop();
  }
}

void KeypadApplet::onStop() {
  _buf = _own;
  _cap = KP_MAX;
  _title = "Text";
  _onConfirm = nullptr;
  _onConfirmCtx = nullptr;
}

void KeypadApplet::drawBuffer(Canvas& c, int x, int y, int w, int h) {
  const mf_font_s* f = fontBody();
  Canvas line = c.region(x, y, w, h);              // clip overflow to the line box
  int fh = line.fontHeight(f);
  int baseY = (h - fh) / 2;
  if (baseY < 0) baseY = 0;

  // Window the text so the cursor stays visible: advance start until the prefix
  // up to the cursor fits in w.
  char prefix[KP_MAX + 1];
  uint16_t start = 0;
  while (start < _cursor) {
    uint16_t n = _cursor - start;
    memcpy(prefix, _buf + start, n); prefix[n] = 0;
    if (line.textWidth(f, prefix) <= w - 2) break;
    start++;
  }
  line.drawText(f, 0, baseY, _buf + start, DisplayDriver::LIGHT);

  // Cursor bar at the cursor's x.
  uint16_t pn = _cursor - start;
  memcpy(prefix, _buf + start, pn); prefix[pn] = 0;
  int cx = line.textWidth(f, prefix);
  line.fillRect(cx, baseY, 1, fh, DisplayDriver::LIGHT);

  // Underline the pending char.
  if (_pending && _pendingPos >= start) {
    uint16_t un = _pendingPos - start;
    memcpy(prefix, _buf + start, un); prefix[un] = 0;
    int ux = line.textWidth(f, prefix);
    char one[2] = { _buf[_pendingPos], 0 };
    line.fillRect(ux, baseY + fh, line.textWidth(f, one), 1, DisplayDriver::LIGHT);
  }
}

void KeypadApplet::configure(char* dst, uint16_t cap, const char* title,
                             KeypadConfirmFn onConfirm, void* ctx) {
  _buf = dst; _cap = cap; _title = title;
  _onConfirm = onConfirm; _onConfirmCtx = ctx;
  _len = (uint16_t)strlen(dst); _cursor = _len;
}

KeypadApplet& keypadApplet() {
  static KeypadApplet s_keypad;
  return s_keypad;
}
MISHMESH_REGISTER_APPLET(&keypadApplet(), ::mishmesh::Placement::AppMenu, "Keypad", 0);

}  // namespace mishmesh
