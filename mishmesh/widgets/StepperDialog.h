#pragma once

#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

// Formats a stepper value into a human label (e.g. raw hop count -> "Direct").
typedef void (*StepperFormatFn)(int value, char* out, uint16_t cap);

enum class StepperResult : uint8_t { None, Confirmed, Cancelled };

// Modal value picker drawn as an overlay (dithered scrim + box), mirroring
// ConfirmDialog. NavLeft/NavRight step the value within [min,max] (no wrap);
// Select confirms, Back/Cancel dismisses. The owner draws its screen, then this
// on top, polls result(), and resets.
class StepperDialog : public Widget {
  const char*     _title;
  int             _value, _min, _max;
  StepperFormatFn _fmt;
  StepperResult   _result;
public:
  StepperDialog()
    : _title(""), _value(0), _min(0), _max(0), _fmt(nullptr),
      _result(StepperResult::None) {}

  void configure(const char* title, int value, int minV, int maxV,
                 StepperFormatFn fmt = nullptr) {
    _title = title ? title : "";
    _min = minV; _max = maxV;
    _value = value < minV ? minV : value > maxV ? maxV : value;
    _fmt = fmt;
    _result = StepperResult::None;
  }
  int value() const { return _value; }
  StepperResult result() const { return _result; }
  void reset() { _result = StepperResult::None; }

  bool onInput(InputEvent ev) override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
