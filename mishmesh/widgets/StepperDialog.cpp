#include <mishmesh/widgets/StepperDialog.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

bool StepperDialog::onInput(InputEvent ev) {
  switch (ev) {
    case InputEvent::NavLeft:  if (_value > _min) _value--; return true;
    case InputEvent::NavRight: if (_value < _max) _value++; return true;
    case InputEvent::Select:   _result = StepperResult::Confirmed; return true;
    case InputEvent::Back:
    case InputEvent::Cancel:   _result = StepperResult::Cancelled; return true;
    default: return false;
  }
}

void StepperDialog::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  Canvas box = drawModalChrome(view);
  int bw = box.width(), bh = box.height();
  int pad = 4;

  box.drawText(fontBody(), pad, pad + 2, _title, DisplayDriver::LIGHT);

  char label[16];
  if (_fmt) _fmt(_value, label, sizeof(label));
  else      snprintf(label, sizeof(label), "%d", _value);

  // Centered value row flanked by arrows; arrows dim at the limits.
  int midY = bh / 2;
  DisplayDriver::Color lcol = _value > _min ? DisplayDriver::LIGHT : DisplayDriver::DARK;
  DisplayDriver::Color rcol = _value < _max ? DisplayDriver::LIGHT : DisplayDriver::DARK;
  box.drawText(fontBody(), pad, midY, "<", lcol, TextAlign::Left);
  box.drawText(fontBody(), bw - pad, midY, ">", rcol, TextAlign::Right);
  box.drawTextEllipsized(fontBody(), bw / 2, midY, bw - 6 * pad, label,
                         DisplayDriver::LIGHT, TextAlign::Center);
}

}  // namespace mishmesh
