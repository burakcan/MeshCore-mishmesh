#include <mishmesh/widgets/IconGrid.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void IconGrid::setModel(const ListModel* m) {
  if (m == _model) return;   // re-binding the same model keeps position
  _model = m;
  _sel = 0;
  _lastSel = -1;
  _scrollPx = _scrollTarget = _barY = 0;
  _animReady = false;
}

bool IconGrid::onInput(InputEvent ev) {
  int n = _model ? _model->count() : 0;
  if (n <= 0) return false;
  if (_sel >= n) _sel = n - 1;          // clamp a stale selection first
  int p = _sel;
  switch (ev) {
    case InputEvent::NavDown:
    case InputEvent::NavRight:
      _sel = (p + 1) % n;
      if (_sel < p) _animReady = false;   // wrap snaps; a full-list slide would look wrong
      return true;
    case InputEvent::NavUp:
    case InputEvent::NavLeft:
      _sel = (p - 1 + n) % n;
      if (_sel > p) _animReady = false;
      return true;
    default:
      return false;
  }
}

void IconGrid::measure(int& w, int& h) const {
  w = 0;
  h = _model ? _model->count() * _rowH : 0;
}

void IconGrid::draw(Canvas& c, int x, int y, int w, int h) {
  if (!_model) return;
  int n = _model->count();
  if (n <= 0) return;
  if (_sel >= n) _sel = n - 1;
  if (_sel != _lastSel) { _marquee.reset(); _lastSel = _sel; }

  uint32_t now = c.now();
  Canvas view = c.region(x, y, w, h);
  int contentH = n * _rowH;
  int maxScroll = contentH > h ? contentH - h : 0;

  // Keep the selected row visible (computed from the committed target, not the
  // eased value, so it doesn't chatter while settling).
  int selTop = _sel * _rowH;
  if (selTop + _rowH > _scrollTarget + h) _scrollTarget = selTop + _rowH - h;
  if (selTop < _scrollTarget) _scrollTarget = selTop;
  if (_scrollTarget > maxScroll) _scrollTarget = maxScroll;
  if (_scrollTarget < 0) _scrollTarget = 0;

  if (!_animReady) { _scrollPx = _scrollTarget; _barY = selTop; _animReady = true; }
  else {
    int minStep = _rowH / 2; if (minStep < 3) minStep = 3;
    _scrollPx = approach(_scrollPx, _scrollTarget, minStep);
    _barY = approach(_barY, selTop, minStep);
  }
  _animating = (_scrollPx != _scrollTarget) || (_barY != selTop);

  bool scrollbar = contentH > h;
  int cw = scrollbar ? w - 4 : w;

  // Base pass: every visible row, un-highlighted.
  for (int i = 0; i < n; i++) {
    int ry = i * _rowH - _scrollPx;
    if (ry + _rowH <= 0 || ry >= h) continue;
    drawRowContent(view, i, ry, cw, DisplayDriver::LIGHT, now);
  }

  // Rounded highlight pill at the eased bar position, then the row(s) it covers
  // re-drawn inverted and clipped to it, so content inverts as the pill glides.
  int barY = _barY - _scrollPx;
  view.fillRoundRect(1, barY + 1, cw - 2, _rowH - 2, DisplayDriver::LIGHT);
  for (int i = 0; i < n; i++) {
    int ry = i * _rowH - _scrollPx;
    if (ry + _rowH <= barY || ry >= barY + _rowH) continue;   // no overlap with the pill
    Canvas bar = view.region(0, barY, cw, _rowH);
    drawRowContent(bar, i, ry - barY, cw, DisplayDriver::DARK, now);
  }

  if (scrollbar) {
    int thumbH = h * h / contentH; if (thumbH < 3) thumbH = 3;
    int thumbY = maxScroll > 0 ? (h - thumbH) * _scrollPx / maxScroll : 0;
    view.fillRect(w - 2, thumbY, 2, thumbH, DisplayDriver::LIGHT);
  }
}

// One row drawn in colour `col` (LIGHT on the dark base pass, DARK for the
// inverted re-draw clipped to the highlight). The icon keeps a black-tile look
// in both states: an outline on the dark screen, a filled black box on the
// light pill - so the icon background stays black even when the row is active.
void IconGrid::drawRowContent(Canvas& view, int i, int ry, int cw,
                              DisplayDriver::Color col, uint32_t now) {
  const int BOX = 16, ICON_PX = 12, MARGIN = 2;
  bool inverted = (col == DisplayDriver::DARK);   // drawing over the light pill

  int boxX = MARGIN;
  int boxY = ry + (_rowH - BOX) / 2;
  if (inverted) view.fillRoundRect(boxX, boxY, BOX, BOX, DisplayDriver::DARK);
  else          view.drawRoundRect(boxX, boxY, BOX, BOX, DisplayDriver::LIGHT);
  uint16_t ic = _model->icon(i);
  if (ic) view.drawGlyph(iconFont(), boxX + (BOX - ICON_PX) / 2,
                         boxY + (BOX - ICON_PX) / 2, ic, DisplayDriver::LIGHT);

  int textX = boxX + BOX + 4;
  int rightW = 0;

  const char* val = _model->value(i);
  if (val && *val) {
    // Badge bubble at the right edge, inverted against the row's colour.
    DisplayDriver::Color bubText = inverted ? DisplayDriver::LIGHT : DisplayDriver::DARK;
    int bw = view.textWidth(fontCaption(), val) + 3;
    int bh = view.fontHeight(fontCaption()) + 2;
    int bx = cw - MARGIN - bw;
    int by = ry + (_rowH - bh) / 2;
    view.fillRect(bx, by, bw, bh, col);
    view.drawText(fontCaption(), bx + 2, by + 1, val, bubText);
    rightW = bw + 4;
  }

  const char* lbl = _model->label(i);
  if (lbl && lbl[0]) {
    int availW = cw - MARGIN - rightW - textX; if (availW < 0) availW = 0;
    int capH = view.fontHeight(fontBody());
    int capY = ry + (_rowH - capH) / 2;
    // Selected + overflow marquees; everything else is left-aligned + ellipsized.
    if (i == _sel && view.textWidth(fontBody(), lbl) > availW)
      _marquee.draw(view, fontBody(), textX, capY, availW, capH, lbl, col, now);
    else
      view.drawTextEllipsized(fontBody(), textX, capY, availW, lbl, col);
  }
}

}  // namespace mishmesh
