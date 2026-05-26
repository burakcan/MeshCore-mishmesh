#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Toggle.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

// Horizontal scroll offset (px) for a marquee of an overflowing label, given ms
// elapsed since the row became selected: hold (start delay), scroll to reveal
// the tail, hold, then repeat.
static int marqueeOffset(uint32_t elapsed, int overflow) {
  const uint32_t PAUSE = 1100;      // ms held at each end (also the start delay)
  const uint32_t PER_PX = 28;       // ms per pixel scrolled
  uint32_t scrollMs = (uint32_t)overflow * PER_PX;
  uint32_t cycle = PAUSE + scrollMs + PAUSE;
  uint32_t t = elapsed % cycle;
  if (t < PAUSE) return 0;
  if (t < PAUSE + scrollMs) return (int)((t - PAUSE) / PER_PX);
  return overflow;
}

void ListMenu::setModel(const ListModel* m) {
  _model = m;
  _selected = 0;
}

bool ListMenu::onInput(InputEvent ev) {
  int n = _model ? _model->count() : 0;
  if (n <= 0) return false;
  if (ev == InputEvent::NavDown) {
    if (_selected < n - 1) _selected++;
    return true;
  }
  if (ev == InputEvent::NavUp) {
    if (_selected > 0) _selected--;
    return true;
  }
  return false;
}

void ListMenu::measure(int& w, int& h) const {
  w = 0;
  h = _rowH * (_model ? _model->count() : 0);
}

// Keep the selection on screen without overscrolling past the last row.
int ListMenu::firstVisibleRow(int box_height) const {
  int n = _model ? _model->count() : 0;
  int rows = box_height / _rowH;
  if (rows < 1) rows = 1;
  int top = _selected - rows + 1;
  if (top < 0) top = 0;
  if (top > n - rows) top = n - rows;
  if (top < 0) top = 0;
  return top;
}

void ListMenu::draw(Canvas& c, int x, int y, int w, int h) {
  _animating = false;
  if (!_model) return;
  int n = _model->count();
  int rows = h / _rowH;
  if (rows < 1) rows = 1;
  int top = firstVisibleRow(h);

  // Restart the marquee phase when the selection changes or the list was
  // off-screen (re-entered after a render gap), so it always begins with the
  // start delay rather than mid-scroll.
  uint32_t now = c.now();
  bool reentered = (_lastDraw == 0) || (now - _lastDraw > 300);
  if (reentered || _selected != _lastSel) _selStart = now;
  _lastSel = _selected;
  _lastDraw = now;
  uint32_t selElapsed = now - _selStart;

  Canvas view = c.region(x, y, w, h);
  int ty = (_rowH - view.fontHeight(fontBody())) / 2;
  if (ty < 0) ty = 0;

  bool scrollbar = n > rows;
  int cw = scrollbar ? w - 4 : w;   // content width reserved from the scrollbar gutter

  for (int i = top; i < n && i < top + rows; i++) {
    int ry = (i - top) * _rowH;
    bool sel = (i == _selected);
    DisplayDriver::Color col = sel ? DisplayDriver::DARK : DisplayDriver::LIGHT;
    if (sel) view.fillRect(0, ry, cw, _rowH, DisplayDriver::LIGHT);

    int tx = 4;
    uint16_t ic = _model->icon(i);
    if (ic) {
      int iconH = view.fontHeight(iconFont());
      int iy = ry + (_rowH - iconH) / 2;
      if (iy < ry) iy = ry;
      view.drawGlyph(iconFont(), 2, iy, ic, col);
      tx = 2 + iconH + 3;
    }

    // Right column: a toggle pill or right-aligned value text.
    int rightW = 0;
    if (_model->isToggle(i)) {
      const int PILL_W = 28;
      int ph = _rowH - 2;
      Toggle pill;
      pill.setOn(_model->toggleState(i));
      pill.setColor(col);
      pill.draw(view, cw - PILL_W - 2, ry + 1, PILL_W, ph);
      rightW = PILL_W + 4;
    } else {
      const char* val = _model->value(i);
      if (val && *val) {
        rightW = view.textWidth(fontBody(), val) + 6;
        view.drawTextEllipsized(fontBody(), cw - 2, ry + ty, cw / 2, val, col, TextAlign::Right);
      }
    }

    // Label, marqueed when the selected row overflows, else ellipsized.
    int availW = cw - tx - rightW;
    if (availW < 0) availW = 0;
    const char* lbl = _model->label(i);
    int tw = view.textWidth(fontBody(), lbl);
    if (sel && tw > availW) {
      _animating = true;
      int overflow = tw - availW + 6;
      int off = marqueeOffset(selElapsed, overflow);
      Canvas clip = view.region(tx, ry, availW, _rowH);
      clip.drawText(fontBody(), -off, ty, lbl, col);
    } else {
      view.drawTextEllipsized(fontBody(), tx, ry + ty, availW, lbl, col);
    }
  }

  if (scrollbar) {
    int thumbH = h * rows / n; if (thumbH < 4) thumbH = 4;
    int thumbY = (n > rows) ? (h - thumbH) * top / (n - rows) : 0;
    view.fillRect(w - 2, thumbY, 2, thumbH, DisplayDriver::LIGHT);
  }
}

}  // namespace mishmesh
