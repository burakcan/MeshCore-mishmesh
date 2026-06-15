#pragma once

#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

enum class ConfirmResult : uint8_t { None, Confirmed, Cancelled };

// Modal confirm dialog drawn as a true overlay (dithered scrim + box). The
// owner draws its normal screen, then this on top; it polls result() and resets.
class ConfirmDialog : public Widget {
  const char*   _msg;
  int           _sel;      // 0 = Cancel, 1 = Confirm; opens on Confirm
  ConfirmResult _result;
public:
  ConfirmDialog() : _msg(""), _sel(1), _result(ConfirmResult::None) {}
  void configure(const char* msg) {
    _msg = msg ? msg : "";
    _sel = 1; _result = ConfirmResult::None;
  }
  ConfirmResult result() const { return _result; }
  void reset() { _sel = 1; _result = ConfirmResult::None; }
  bool onInput(InputEvent ev) override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
