#pragma once

#include <stdint.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/GridView.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

typedef void (*KeypadConfirmFn)(void* ctx, const char* text);

class Canvas;

// Nokia-3310-style multi-tap text entry on a 4x4 GridView. Standalone for now
// (menu launch edits an internal buffer; configure() lets a future caller edit
// its own). Driven entirely off semantic InputEvents.
class KeypadApplet : public Applet, public GridModel {
public:
  static const uint16_t KP_MAX = 160;          // matches message char limit
  static const uint32_t TAP_TIMEOUT_MS = 800;  // multi-tap commit timeout

  // Shift = "Abc": one-shot capitalize - the next letter is upper, then it
  // reverts to Lower automatically once that letter is committed.
  enum class Mode : uint8_t { Lower, Shift, Upper, Num };

  KeypadApplet();

  // GridModel
  int rows() const override { return 4; }
  int cols() const override { return 4; }
  const char* cellLabel(int r, int c) const override;
  uint16_t cellIcon(int r, int c) const override;

  // Applet
  void onStart(AppletContext& ctx) override;
  void onStop() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Future integration seam (unused by the menu launch). Call before push().
  // onConfirm (if set) receives the buffer on OK, before the host pops.
  void configure(char* dst, uint16_t cap, const char* title,
                 KeypadConfirmFn onConfirm = nullptr, void* ctx = nullptr);

  // Locks the keypad to numeric entry: digits + '.' + '-', letter/sym modes off.
  // Reuses the configure() buffer/onConfirm seam. Call before push().
  void configureNumeric(char* dst, uint16_t cap, const char* title,
                        KeypadConfirmFn onConfirm = nullptr, void* ctx = nullptr);

  void setFocusForTest(int r, int c) { _grid.setFocus(r, c); }

  // Accessors / logic (also used by tests).
  const char* text() const { return _buf; }
  uint16_t length() const { return _len; }
  uint16_t cursor() const { return _cursor; }
  Mode mode() const { return _mode; }
  bool symPage() const { return _symPage; }
  void cycleMode();      // Lower -> Upper -> Num -> Lower; exits sym page
  void toggleSymPage();  // letters <-> symbols

private:
  AppletContext* _ctx;
  char  _own[KP_MAX + 1];  // the working buffer; the keypad only ever edits this
  char* _buf;            // always points at _own (kept for call-site brevity)
  char* _src;            // configure() destination; written only on OK. null = standalone
  uint16_t _cap;
  const char* _title;
  KeypadConfirmFn _onConfirm;
  void* _onConfirmCtx;
  uint16_t _len;
  uint16_t _cursor;      // insertion index, 0.._len
  Mode _mode;
  bool _symPage;
  bool _numericOnly = false;   // configureNumeric(): digits + . - only, no mode switch
  GridView _grid;

  // pending multi-tap state
  bool _pending;
  uint16_t _pendingPos;  // index in _buf of the cycling char
  int _tapGroupCell;     // char-cell index 0..8 currently cycling, -1 = none
  uint8_t _tapIndex;     // position within the group
  uint32_t _lastTapMs;
  bool _tapStampPending; // a tap occurred; (re)stamp _lastTapMs on the next render (fresh clock)
  uint32_t _now;         // cached frame clock, for input-time timing

  // Discard-changes guard. Back exits the keypad (like everywhere else); if the
  // working buffer differs from the untouched source, confirm before dropping it.
  ConfirmDialog _confirm;
  bool _confirming;

  bool isDirty() const;
  const char* groupAt(int charIndex) const;   // group string for char cell 0..8
  void insertCharAt(uint16_t pos, char ch);
  void deleteCharAt(uint16_t pos);
  void commitPending();
  void handleSelect();
  void handleSelectLong();   // long-press: type the focused cell's digit (1..9, or 0)
  void handleCharCell(int charIndex);
  void confirmAndExit();
  void drawBuffer(Canvas& c, int x, int y, int w, int h);
};

KeypadApplet& keypadApplet();

}  // namespace mishmesh
