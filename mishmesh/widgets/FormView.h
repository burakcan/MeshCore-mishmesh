#pragma once
#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;

// Pre-formatted field row: the owner resolves the display value before calling draw().
struct FormRow { const char* label; const char* value; };

// Pure form renderer: label+boxed-value fields then an optional centred button.
// Owns focus and scroll state; no Select handling (the owner reads focus() and acts).
// Host-safe: no Arduino/hardware dependencies.
class FormView {
  int _focus = 0, _scroll = 0;
public:
  void reset() { _focus = 0; _scroll = 0; }
  int  focus() const { return _focus; }   // 0..n-1 = fields, n = button (if hasButton)
  void setFocus(int f) { _focus = f; }

  // Draw label+box rows + (if submitLabel != nullptr) a centred button into the
  // sub-region (x,y,w,h). Scroll is adjusted to keep the focused item visible.
  void draw(Canvas& c, int x, int y, int w, int h,
            const FormRow* rows, int n, const char* submitLabel);

  // NavUp/NavDown move focus within [0 .. n-1 + (hasButton?1:0)]; returns true
  // if the event was consumed. No Select handling.
  bool onInput(InputEvent ev, int n, bool hasButton);
};

}  // namespace mishmesh
