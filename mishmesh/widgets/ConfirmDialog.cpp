#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

bool ConfirmDialog::onInput(InputEvent ev) {
  switch (ev) {
    case InputEvent::NavLeft:  _sel = 0; return true;
    case InputEvent::NavRight: _sel = 1; return true;
    case InputEvent::Select:   _result = _sel == 1 ? ConfirmResult::Confirmed : ConfirmResult::Cancelled; return true;
    case InputEvent::Back:     _result = ConfirmResult::Cancelled; return true;
    default: return false;
  }
}

void ConfirmDialog::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  view.fillStipple(0, 0, w, h, DisplayDriver::DARK);   // scrim dims content behind

  int bw = w - 16, bh = h - 16;
  int bx = (w - bw) / 2, by = (h - bh) / 2;
  view.fillStipple(bx + 2, by + 2, bw, bh, DisplayDriver::DARK);  // dithered drop shadow, behind the box

  Canvas box = view.region(bx, by, bw, bh);
  box.fillRect(0, 0, bw, bh, DisplayDriver::DARK);
  box.drawRect(0, 0, bw, bh, DisplayDriver::LIGHT);

  int pad = 4;
  box.drawTextWrapped(fontBody(), pad, pad + 2, bw - 2 * pad, _msg, DisplayDriver::LIGHT);

  // Buttons along the bottom.
  int btnH = box.lineHeight(fontBody()) + 2;
  int btnY = bh - btnH - 2;
  int half = bw / 2;
  for (int i = 0; i < 2; i++) {
    int bxo = i * half;
    bool sel = (_sel == i);
    if (sel) box.fillRect(bxo + 2, btnY, half - 4, btnH, DisplayDriver::LIGHT);
    DisplayDriver::Color col = sel ? DisplayDriver::DARK : DisplayDriver::LIGHT;
    box.drawTextEllipsized(fontBody(), bxo + half / 2, btnY + 1, half - 4,
                           i == 0 ? "Cancel" : "Confirm", col, TextAlign::Center);
  }
}

}  // namespace mishmesh
