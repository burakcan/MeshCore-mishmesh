#include <mishmesh/widgets/FormView.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

// Geometry constants (must match FormApplet's layout exactly).
static const int ROW_H   = 17;
static const int BOX_H   = 14;
static const int BTN_H   = 14;
static const int BTN_GAP = 5;
static const int PAD     = 4;

void FormView::draw(Canvas& c, int x, int y, int w, int h,
                    const FormRow* rows, int n, const char* submitLabel) {
  // Draw into a CLIPPED sub-region (local origin) so scrolled rows can't bleed
  // above/below the form area (e.g. into the StatusBar). All coords below are local.
  Canvas view = c.region(x, y, w, h);
  const mf_font_s* lf = fontCaption();
  const mf_font_s* vf = fontBody();

  // Button geometry (content-relative, before scroll) + total content height.
  const int btnTop = n * ROW_H + BTN_GAP;
  const int contentH = submitLabel ? (btnTop + BTN_H) : (n > 0 ? (n - 1) * ROW_H + BOX_H : 0);

  // Scroll to keep the focused item visible.
  int focTop, focBot;
  if (_focus < n) {
    focTop = _focus * ROW_H;
    focBot = focTop + BOX_H;
  } else if (submitLabel) {
    focTop = btnTop;
    focBot = focTop + BTN_H;
  } else {
    focTop = 0; focBot = 0;
  }
  if (focTop - _scroll < 0)      _scroll = focTop;
  else if (focBot - _scroll > h) _scroll = focBot - h;
  int maxScroll = contentH - h;
  if (maxScroll < 0) maxScroll = 0;
  if (_scroll > maxScroll) _scroll = maxScroll;
  if (_scroll < 0)         _scroll = 0;

  const bool scroll = contentH > h;
  const int  gutter = scroll ? 3 : 0;   // reserve a right-edge column for the scrollbar

  // Label column = widest label, clamped to a third of the region width.
  int labelW = 0;
  for (int i = 0; i < n; i++) {
    if (!rows[i].label) continue;
    int lw = view.textWidth(lf, rows[i].label);
    if (lw > labelW) labelW = lw;
  }
  const int maxLabel = w / 3;
  if (labelW > maxLabel) labelW = maxLabel;

  const int boxX = PAD + labelW + PAD;
  const int boxW = w - boxX - PAD - gutter;

  int btnW = 0, btnX = 0;
  if (submitLabel) {
    btnW = view.textWidth(vf, submitLabel) + 18;
    if (btnW > w - 2 * PAD - gutter) btnW = w - 2 * PAD - gutter;
    btnX = (w - gutter - btnW) / 2;
  }

  const int lh = view.fontHeight(lf);
  const int vh = view.fontHeight(vf);

  // Fields (local coords; the region clips any overflow at top/bottom).
  for (int i = 0; i < n; i++) {
    const int rowY = i * ROW_H - _scroll;
    if (rowY + BOX_H < 0 || rowY > h) continue;   // fully off-screen
    const bool foc = (_focus == i);
    if (rows[i].label)
      view.drawText(lf, PAD, rowY + (BOX_H - lh) / 2, rows[i].label, DisplayDriver::LIGHT);
    if (foc) view.fillRect(boxX, rowY, boxW, BOX_H, DisplayDriver::LIGHT);
    else     view.drawRect(boxX, rowY, boxW, BOX_H, DisplayDriver::LIGHT);
    const char* val = rows[i].value;
    if (val && val[0]) {
      view.drawTextEllipsized(vf, boxX + 3, rowY + (BOX_H - vh) / 2, boxW - 6, val,
                              foc ? DisplayDriver::DARK : DisplayDriver::LIGHT);
    }
  }

  // Button (centred; inverted when focused).
  if (submitLabel) {
    const int btnY = btnTop - _scroll;
    const bool bfoc = (_focus == n);
    if (bfoc) view.fillRect(btnX, btnY, btnW, BTN_H, DisplayDriver::LIGHT);
    else      view.drawRect(btnX, btnY, btnW, BTN_H, DisplayDriver::LIGHT);
    view.drawTextEllipsized(vf, btnX + btnW / 2, btnY + (BTN_H - vh) / 2, btnW - 6,
                            submitLabel, bfoc ? DisplayDriver::DARK : DisplayDriver::LIGHT,
                            TextAlign::Center);
  }

  // Scrollbar thumb (right edge), mirroring ScrollText.
  if (scroll) {
    view.drawScrollbarThumb(w - 2, h, contentH, _scroll);
  }
}

bool FormView::onInput(InputEvent ev, int n, bool hasButton) {
  switch (ev) {
    case InputEvent::NavUp:
      if (_focus > 0) _focus--;
      return true;
    case InputEvent::NavDown:
      if (_focus < (hasButton ? n : n - 1)) _focus++;
      return true;
    default:
      return false;
  }
}

}  // namespace mishmesh
