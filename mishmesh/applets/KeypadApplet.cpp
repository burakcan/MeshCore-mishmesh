#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/text/KeyboardLayouts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

KeypadApplet::KeypadApplet()
    : Applet("Keypad"), _ctx(nullptr), _buf(_own), _src(nullptr), _cap(KP_MAX), _title("Text"),
      _len(0), _cursor(0), _mode(Mode::Lower), _symPage(false),
      _pending(false), _pendingPos(0), _tapGroupCell(-1), _tapIndex(0),
      _lastTapMs(0), _tapStampPending(false), _now(0), _confirming(false),
      _onConfirm(nullptr), _onConfirmCtx(nullptr) {
  _own[0] = 0;
}

bool KeypadApplet::isDirty() const {
  const char* orig = _src ? _src : "";   // standalone: original was empty
  return strcmp(_buf, orig) != 0;
}

const char* KeypadApplet::groupAt(int i) const {
  static const char* const NUM[9] = {"1","2","3","4","5","6","7","8","9"};
  static const char* const SYM[9] = {".,?!",":;\"'","()[]","<>{}","+-*/","=_~","@#&","%$|","^`\\"};
  if (i < 0 || i > 8) return "";
  if (_symPage) return SYM[i];
  if (_mode == Mode::Num) return NUM[i];
  const KbdLayout& L = kbdLayoutAt(_langIdx);
  if (_mode == Mode::Upper || _mode == Mode::Shift) return L.upper[i];  // Shift shows caps for the one-shot
  return L.lower[i];
}

// Advance one UTF-8 codepoint; returns byte length of the codepoint at s.
static int cpLen(const char* s) {
  unsigned char b = (unsigned char)*s;
  if (b < 0x80) return 1;
  if ((b >> 5) == 0x6)  return 2;
  if ((b >> 4) == 0xE)  return 3;
  if ((b >> 3) == 0x1E) return 4;
  return 1;
}

int KeypadApplet::groupCpCount(const char* g) const {
  int n = 0;
  for (const char* p = g; *p; p += cpLen(p)) n++;
  return n;
}

const char* KeypadApplet::groupCpAt(const char* g, int i, int* len) const {
  const char* p = g;
  for (int k = 0; k < i && *p; k++) p += cpLen(p);
  if (len) *len = *p ? cpLen(p) : 0;
  return p;
}

const char* KeypadApplet::cellLabel(int r, int c) const {
  if (_emojiPage) {
    if (r < 3) return _emojiCells[r * 4 + c];   // UTF-8 glyph (or "")
    if (c == 0) return "abc";
    return (c == 3) ? "OK" : "";                // c1/c2 use icons
  }
  if (r < 3 && c < 3) {
    int i = r * 3 + c;
    // Symbols, digits, and the punctuation cell fit and are shown verbatim.
    // Letter cells show only the base Latin labels (like real Nokia keypads) -
    // the language's accented variants still cycle when typing, they just don't
    // fit on the key and aren't drawn.
    if (_symPage || _mode == Mode::Num || i == 0) return groupAt(i);
    static const char* const BASE_L[9] = {"", "abc","def","ghi","jkl","mno","pqrs","tuv","wxyz"};
    static const char* const BASE_U[9] = {"", "ABC","DEF","GHI","JKL","MNO","PQRS","TUV","WXYZ"};
    return (_mode == Mode::Upper || _mode == Mode::Shift) ? BASE_U[i] : BASE_L[i];
  }
  if (c == 3) {
    if (r == 0) return "DEL";
    if (r == 1) return _numericOnly ? "-" : "SPC";
    if (r == 2) {  // shift cell: mode toggle, or '.' in numeric mode
      if (_numericOnly) return ".";
      return _mode == Mode::Upper ? "AB"
           : _mode == Mode::Shift ? "Ab"
           : _mode == Mode::Num ? "12" : "ab";
    }
  }
  // r == 3
  if (c == 0) {
    // Label shows the NEXT state the button cycles to (see cycleBottomLeft):
    // letters -> sym -> emoji (when a catalog is registered) -> letters.
    if (_symPage) return emojiCatalogCount() > 0 ? "emo" : "abc";
    if (_mode == Mode::Num) return "0";
    return "sym";
  }
  if (c == 1) return "<";   // cursor left  (icon fallback; Task 5 adds glyph)
  if (c == 2) return ">";   // cursor right
  return "OK";              // c == 3 confirm
}

uint16_t KeypadApplet::cellIcon(int r, int c) const {
  if (_emojiPage) {
    if (r < 3) return 0;                                  // emoji drawn as text label
    if (c == 1) return (uint16_t)Icon::ArrowLeft;
    if (c == 2) return (uint16_t)Icon::ArrowRight;
    if (c == 3) return (uint16_t)Icon::Check;
    return 0;                                             // c0 = "abc" text
  }
  if (r < 3 && c < 3) return 0;                              // char groups: text
  if (c == 3 && r == 0) return (uint16_t)Icon::Backspace;   // DEL
  if (r == 3 && c == 1) return (uint16_t)Icon::ArrowLeft;   // cursor left (existing icon)
  if (r == 3 && c == 2) return (uint16_t)Icon::ArrowRight;  // cursor right
  if (r == 3 && c == 3) return (uint16_t)Icon::Check;       // OK
  return 0;                                                  // SPC / shift / sym: text
}

void KeypadApplet::commitPending() {
  // "Abc" one-shot: revert to lower once the shifted letter is finalized. Only on
  // a real pending letter - bare navigation in Shift mode must keep the shift armed.
  if (_pending && _mode == Mode::Shift) _mode = Mode::Lower;
  _pending = false;
  _tapGroupCell = -1;
}

void KeypadApplet::cycleMode() {
  if (_numericOnly) return;
  commitPending();
  _symPage = false;
  _mode = _mode == Mode::Lower ? Mode::Shift    // abc -> Abc -> ABC -> 123 -> abc
        : _mode == Mode::Shift ? Mode::Upper
        : _mode == Mode::Upper ? Mode::Num : Mode::Lower;
}

void KeypadApplet::toggleSymPage() {
  if (_numericOnly) return;
  commitPending();
  _symPage = !_symPage;
  if (_symPage) _mode = Mode::Lower;   // symbols are case-independent; normalize
}

int KeypadApplet::emojiPageCount() const {
  int n = emojiCatalogCount();
  return n > 0 ? (n + 11) / 12 : 0;
}

const char* KeypadApplet::langCode() const { return kbdLayoutAt(_langIdx).code; }

void KeypadApplet::setLanguageByIndex(int i) {
  int n = kbdLayoutCount();
  if (i < 0) i = 0;
  if (i >= n) i = n - 1;
  _langIdx = (uint8_t)i;
}

bool KeypadApplet::setLanguageByCode(const char* code) {
  int i = kbdLayoutIndexByCode(code);
  if (i < 0) return false;
  _langIdx = (uint8_t)i;
  return true;
}

int KeypadApplet::LangListModel::count() const { return kbdLayoutCount(); }
const char* KeypadApplet::LangListModel::label(int i) const { return kbdLayoutAt(i).name; }

void KeypadApplet::openLanguagePicker() {
  _langModel.active = _langIdx;
  _langMenu.setModel(&_langModel);
  _langMenu.resetSelection();          // avoid same-ptr no-reset gotcha
  _langMenu.setSelected(_langIdx);
  _langPicking = true;
}

void KeypadApplet::fillEmojiCells() {
  int cnt = emojiCatalogCount();
  for (int i = 0; i < 12; i++) {
    int idx = _emojiPageIdx * 12 + i;
    if (idx < cnt) utf8Encode(emojiCatalogAt((uint16_t)idx), _emojiCells[i]);
    else _emojiCells[i][0] = 0;
  }
}

void KeypadApplet::cycleBottomLeft() {
  if (_numericOnly) return;
  commitPending();
  if (_emojiPage) { _emojiPage = false; return; }            // emoji -> letters
  if (_symPage) {                                            // sym -> emoji (or letters)
    _symPage = false;
    if (emojiCatalogCount() > 0) { _emojiPage = true; _emojiPageIdx = 0; fillEmojiCells(); }
    return;
  }
  _symPage = true; _mode = Mode::Lower;                      // letters -> sym
}

void KeypadApplet::nextEmojiPage() {
  int pc = emojiPageCount(); if (pc <= 1) return;
  _emojiPageIdx = (uint8_t)((_emojiPageIdx + 1) % pc); fillEmojiCells();
}

void KeypadApplet::prevEmojiPage() {
  int pc = emojiPageCount(); if (pc <= 1) return;
  _emojiPageIdx = (uint8_t)((_emojiPageIdx + pc - 1) % pc); fillEmojiCells();
}

void KeypadApplet::insertEmojiCell(int cell) {
  if (cell < 0 || cell >= 12) return;
  const char* s = _emojiCells[cell];
  if (!s[0]) return;
  insertString(_cursor, s);
  _cursor = (uint16_t)(_cursor + strlen(s));
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

int KeypadApplet::utf8Encode(uint32_t cp, char out[5]) {
  int n;
  if (cp < 0x80)        { out[0] = (char)cp; n = 1; }
  else if (cp < 0x800)  { out[0] = (char)(0xC0 | (cp >> 6));
                          out[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
  else if (cp < 0x10000){ out[0] = (char)(0xE0 | (cp >> 12));
                          out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                          out[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
  else                  { out[0] = (char)(0xF0 | (cp >> 18));
                          out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                          out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                          out[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
  out[n] = 0;
  return n;
}

void KeypadApplet::insertString(uint16_t pos, const char* s) {
  uint16_t sl = (uint16_t)strlen(s);
  if (sl == 0 || (uint32_t)_len + sl > _cap) return;
  memmove(_buf + pos + sl, _buf + pos, (size_t)(_len - pos + 1));  // move NUL too
  memcpy(_buf + pos, s, sl);
  _len = (uint16_t)(_len + sl);
}

uint16_t KeypadApplet::prevCodepoint(uint16_t pos) const {
  if (pos == 0) return 0;
  uint16_t i = (uint16_t)(pos - 1);
  while (i > 0 && ((unsigned char)_buf[i] & 0xC0) == 0x80) i--;    // skip continuation bytes
  return i;
}

uint16_t KeypadApplet::nextCodepoint(uint16_t pos) const {
  if (pos >= _len) return _len;
  unsigned char b = (unsigned char)_buf[pos];
  uint16_t adv = (b < 0x80) ? 1 : ((b >> 5) == 0x6) ? 2
               : ((b >> 4) == 0xE) ? 3 : ((b >> 3) == 0x1E) ? 4 : 1;
  uint16_t n = (uint16_t)(pos + adv);
  return n > _len ? _len : n;
}

void KeypadApplet::handleCharCell(int idx) {
  const char* g = groupAt(idx);
  int cpn = groupCpCount(g);
  if (cpn == 0) return;

  if (_pending && _tapGroupCell == idx) {           // cycle within same group
    uint8_t ni = (uint8_t)((_tapIndex + 1) % cpn); // candidate next index - don't commit yet
    int nlen = 0;
    const char* cp = groupCpAt(g, ni, &nlen);
    // Replace the pending glyph: delete the codepoint currently at _pendingPos,
    // then insert the new one there. Widths may differ (1-byte <-> 2-byte).
    uint16_t oldEnd = nextCodepoint(_pendingPos);
    uint16_t ow = (uint16_t)(oldEnd - _pendingPos);
    // Guard: if cycling to a wider codepoint would exceed cap, keep the current
    // glyph and index unchanged - no visible corruption, no bad cursor.
    if ((uint32_t)_len - ow + (uint32_t)nlen > _cap) return;
    _tapIndex = ni;                                  // commit the index advance
    // each deleteCharAt removes one byte and shifts the buffer left; oldEnd
    // counts down the remaining bytes of the codepoint being replaced.
    while (oldEnd > _pendingPos) { deleteCharAt(_pendingPos); oldEnd--; }
    char one[5];
    memcpy(one, cp, (size_t)nlen); one[nlen] = 0;
    insertString(_pendingPos, one);
    _cursor = (uint16_t)(_pendingPos + nlen);
    _tapStampPending = true;          // restart the timer on the next render (fresh clock)
    return;
  }

  commitPending();                                   // Abc one-shot may drop mode to lower here
  g = groupAt(idx);                                  // recompute after commit (2nd letter is lower)
  cpn = groupCpCount(g);
  if (cpn == 0) return;
  int flen = 0;
  const char* first = groupCpAt(g, 0, &flen);
  if ((uint32_t)_len + flen > _cap) return;          // buffer full
  char one[5];
  memcpy(one, first, (size_t)flen); one[flen] = 0;
  _pendingPos = _cursor;
  insertString(_cursor, one);
  _cursor = (uint16_t)(_cursor + flen);
  if (cpn > 1) {                                     // single-cp groups don't cycle
    _pending = true;
    _tapGroupCell = idx;
    _tapIndex = 0;
    _tapStampPending = true;
  }
}

void KeypadApplet::handleSelect() {
  if (_emojiPage) {
    int er = _grid.focusedRow(), ec = _grid.focusedCol();
    if (er < 3) { insertEmojiCell(er * 4 + ec); return; }    // 12 emoji cells
    if (ec == 0) { cycleBottomLeft(); return; }              // "abc" -> letters
    if (ec == 1) { prevEmojiPage(); return; }
    if (ec == 2) { nextEmojiPage(); return; }
    confirmAndExit(); return;                                // OK
  }
  int r = _grid.focusedRow(), c = _grid.focusedCol();
  if (r < 3 && c < 3) { handleCharCell(r * 3 + c); return; }
  if (c == 3) {
    if (r == 0) {   // DEL - remove the whole codepoint before the cursor
      commitPending();
      if (_cursor > 0) {
        uint16_t p = prevCodepoint(_cursor);
        while (_cursor > p) { deleteCharAt(_cursor - 1); _cursor--; }
      }
      return;
    }
    if (r == 1) { commitPending(); insertCharAt(_cursor, _numericOnly ? '-' : ' '); _cursor++; return; }
    if (r == 2) {                                   // shift / mode cycle, or '.' in numeric
      if (_numericOnly) { commitPending(); insertCharAt(_cursor, '.'); _cursor++; }
      else cycleMode();
      return;
    }
  }
  // r == 3
  if (c == 0) {
    if (_mode == Mode::Num) { commitPending(); insertCharAt(_cursor, '0'); _cursor++; return; }
    cycleBottomLeft(); return;   // letters -> sym -> emoji(if any) -> letters
  }
  if (c == 1) { commitPending(); _cursor = prevCodepoint(_cursor); return; }   // cursor left
  if (c == 2) { commitPending(); _cursor = nextCodepoint(_cursor); return; }   // cursor right
  confirmAndExit();                                  // c == 3, OK
}

void KeypadApplet::handleSelectLong() {
  int r = _grid.focusedRow(), c = _grid.focusedCol();
  // The 3x3 letter grid maps cell idx 0..8 -> digits '1'..'9'; the '0'/sym cell
  // (r==3,c==0) types '0'. Everything else falls back to the plain press.
  if (r < 3 && c < 3) {
    commitPending();
    insertCharAt(_cursor, (char)('1' + (r * 3 + c)));
    _cursor++;
    return;
  }
  if (r == 3 && c == 0 && !_numericOnly) {
    commitPending();
    insertCharAt(_cursor, '0');
    _cursor++;
    return;
  }
  handleSelect();
}

void KeypadApplet::onStart(AppletContext& ctx) {
  _ctx = &ctx;
  if (!_langLoaded && _ctx && _ctx->storage) {
    char code[6] = {0};
    uint8_t n = _ctx->storage->load("kbdl", (uint8_t*)code, sizeof(code) - 1);
    code[n] = 0;
    int i = kbdLayoutIndexByCode(code);
    if (i >= 0) _langIdx = (uint8_t)i;
    _langLoaded = true;
  }
  _grid.setModel(this);
  _grid.setFocus(0, 1);            // start on "abc" - the natural first target for typing
  if (!_src) { _own[0] = 0; _len = 0; _cursor = 0; }  // standalone: fresh buffer (configure() already seeded)
  _buf = _own;
  _mode = _numericOnly ? Mode::Num : Mode::Lower;
  _symPage = false;
  _langFocused = false;
  _langPicking = false;
  commitPending();
  _confirming = false;
  _confirm.reset();
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

  char tagbuf[8];
  const char* tag;
  if (_emojiPage) {
    snprintf(tagbuf, sizeof(tagbuf), "%d/%d", _emojiPageIdx + 1, emojiPageCount());
    tag = tagbuf;
    c.drawText(fontCaption(), c.width(), 3, tag, DisplayDriver::LIGHT, TextAlign::Right);
  } else {
    tag = langCode();
    int tw = c.textWidth(fontCaption(), tag);
    int tx = c.width() - tw;
    if (_langFocused) {                    // inverted pill = focused button
      c.fillRect(tx - 2, 0, tw + 3, topH - 1, DisplayDriver::LIGHT);
      c.drawText(fontCaption(), c.width() - 1, 3, tag, DisplayDriver::DARK, TextAlign::Right);
    } else {
      c.drawText(fontCaption(), c.width(), 3, tag, DisplayDriver::LIGHT, TextAlign::Right);
    }
  }

  _grid.setFocusVisible(!_langFocused);   // button owns focus -> grid shows no highlight
  _grid.draw(c, 0, topH, c.width(), c.height() - topH);

  if (_langPicking) {
    // ListMenu doesn't paint its own background; clear first so the keypad
    // underneath doesn't bleed through the list.
    c.fillRect(0, 0, c.width(), c.height(), DisplayDriver::DARK);
    _langMenu.draw(c, 0, 0, c.width(), c.height());
    return _langMenu.needsAnimation() ? ListMenu::TICK_MS : 100;
  }
  if (_confirming) {                       // discard dialog overlays the keypad
    _confirm.draw(c, 0, 0, c.width(), c.height());
    return 100;
  }
  return _pending ? 80 : 1000;
}

bool KeypadApplet::onInput(InputEvent ev) {
  if (_confirming) {                       // discard dialog is modal: route input to it
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        bool discard = (r == ConfirmResult::Confirmed);
        _confirming = false; _confirm.reset();
        if (discard && _ctx && _ctx->host) _ctx->host->pop();   // drop edits, exit
      }
    }
    return true;                           // swallow everything while the dialog is up
  }
  if (_langPicking) {                    // language picker is modal
    if (ev == InputEvent::Back) {        // cancel, keep current language
      _langPicking = false; _langFocused = false; return true;
    }
    if (ev == InputEvent::Select) {      // commit selection
      setLanguageByIndex(_langMenu.selected());
      if (_ctx && _ctx->storage) {
        const char* code = langCode();
        _ctx->storage->save("kbdl", (const uint8_t*)code, (uint8_t)strlen(code));
      }
      _langPicking = false; _langFocused = false;
      return true;
    }
    _langMenu.onInput(ev);               // Nav moves selection
    return true;
  }
  if (_langFocused) {                      // button has focus: modal-ish mini state
    switch (ev) {
      case InputEvent::NavDown:
      case InputEvent::Back:
        _langFocused = false; return true; // return to the grid (do not exit)
      case InputEvent::Select:
        openLanguagePicker(); return true;
      default:
        return true;                       // swallow left/right/up while focused
    }
  }
  switch (ev) {
    case InputEvent::NavLeft:
    case InputEvent::NavRight:
      if (_emojiPage) {
        int nr = _grid.focusedRow(), nc = _grid.focusedCol();
        if (nr < 3 && ev == InputEvent::NavRight && nc == 3) { nextEmojiPage(); return true; }
        if (nr < 3 && ev == InputEvent::NavLeft  && nc == 0) { prevEmojiPage(); return true; }
      }
      commitPending();
      return _grid.onInput(ev);
    case InputEvent::NavUp:
      if (!_emojiPage && _grid.focusedRow() == 0) {  // top row -> language button
        commitPending();
        _langFocused = true;
        return true;
      }
      commitPending();
      return _grid.onInput(ev);
    case InputEvent::NavDown:
      commitPending();
      return _grid.onInput(ev);
    case InputEvent::Select:
      handleSelect();
      return true;
    case InputEvent::SelectLong:            // long-press a letter cell = type its digit
      handleSelectLong();
      return true;
    case InputEvent::Back:                  // Back = exit, like every other screen;
    case InputEvent::BackLong:              // backspace lives on the DEL cell now
      commitPending();
      if (isDirty()) { _confirm.configure("Discard changes?"); _confirming = true; return true; }
      return false;                         // unchanged -> host pops (exit)
    default:
      return false;
  }
}

void KeypadApplet::confirmAndExit() {
  commitPending();
  if (_src && _src != _buf) {            // commit the working copy to the source on OK
    memcpy(_src, _buf, _len);
    _src[_len] = 0;
  }
  const char* result = _src ? _src : _buf;
  if (_onConfirm) _onConfirm(_onConfirmCtx, result);
  if (_ctx && _ctx->host) {
    if (!_onConfirm) _ctx->host->postToast(_len ? "Saved" : "(empty)");
    _ctx->host->pop();
  }
}

void KeypadApplet::onStop() {
  _buf = _own;
  _src = nullptr;
  _cap = KP_MAX;
  _title = "Text";
  _onConfirm = nullptr;
  _onConfirmCtx = nullptr;
  _confirming = false;
  _confirm.reset();
  _numericOnly = false;
}

void KeypadApplet::drawBuffer(Canvas& c, int x, int y, int w, int h) {
  const mf_font_s* f = fontBody();
  Canvas line = c.region(x, y, w, h);              // clip overflow to the line box
  int fh = line.fontHeight(f);
  int baseY = (h - fh) / 2;
  if (baseY < 0) baseY = 0;

  // Empty buffer: show the configured title as a recessive placeholder (smaller
  // font) so the screen says what it wants - e.g. "Room password" vs "Message".
  // It clears on the first keypress. Without this the title is invisible and
  // every keypad looks the same.
  if (_len == 0 && _title && _title[0]) {
    const mf_font_s* hf = fontCaption();
    int hfh = line.fontHeight(hf);
    int hy = (h - hfh) / 2;
    if (hy < 0) hy = 0;
    line.fillRect(0, baseY, 1, fh, DisplayDriver::LIGHT);   // cursor at the start
    line.drawText(hf, 4, hy, _title, DisplayDriver::LIGHT);
    return;
  }

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
    uint16_t gend = nextCodepoint(_pendingPos);
    char glyph[5];
    uint16_t gl = (uint16_t)(gend - _pendingPos);
    if (gl > 4) gl = 4;
    memcpy(glyph, _buf + _pendingPos, gl); glyph[gl] = 0;
    line.fillRect(ux, baseY + fh, line.textWidth(f, glyph), 1, DisplayDriver::LIGHT);
  }
}

void KeypadApplet::configure(char* dst, uint16_t cap, const char* title,
                             KeypadConfirmFn onConfirm, void* ctx) {
  _numericOnly = false;
  _src = dst;                                  // written only on OK; never during editing
  _cap = cap < KP_MAX ? cap : KP_MAX;
  _title = title;
  _onConfirm = onConfirm; _onConfirmCtx = ctx;
  _buf = _own;                                 // seed the working copy from the source
  uint16_t n = 0;
  if (dst) while (dst[n] && n < _cap) { _own[n] = dst[n]; n++; }
  _own[n] = 0;
  _len = n; _cursor = n;
}

void KeypadApplet::configureNumeric(char* dst, uint16_t cap, const char* title,
                                    KeypadConfirmFn onConfirm, void* ctx) {
  configure(dst, cap, title, onConfirm, ctx);
  _numericOnly = true;
}

KeypadApplet& keypadApplet() {
  static KeypadApplet s_keypad;
  return s_keypad;
}

}  // namespace mishmesh
