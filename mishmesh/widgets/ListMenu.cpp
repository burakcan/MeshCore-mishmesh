#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Toggle.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void ListMenu::setModel(const ListModel* m) {
  if (m == _model) return;   // re-binding the same list (e.g. returning from a drill-in) keeps position
  _model = m;
  _selected = 0;
  _scrollPx = 0;
}

bool ListMenu::onInput(InputEvent ev) {
  int n = _model ? _model->count() : 0;
  if (n <= 0) return false;
  if (_selected >= n) _selected = n - 1;        // clamp a stale selection first
  if (ev == InputEvent::NavDown) { _selected = (_selected + 1) % n; return true; }
  if (ev == InputEvent::NavUp)   { _selected = (_selected - 1 + n) % n; return true; }
  return false;
}

void ListMenu::measure(int& w, int& h) const {
  w = 0;
  h = _headerH + _rowH * (_model ? _model->count() : 0);
}

// Kept for callers that need the row-based scroll offset (and tests).
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

void ListMenu::drawRow(Canvas& view, int i, int ry, int cw, uint32_t now) {
  bool sel = (i == _selected);
  DisplayDriver::Color col = sel ? DisplayDriver::DARK : DisplayDriver::LIGHT;
  if (sel) view.fillRect(0, ry, cw, _rowH, DisplayDriver::LIGHT);

  int tx = 4;
  uint16_t ic = _model->icon(i);
  if (ic) {
    int iconH = view.fontHeight(iconFont());
    int iy = ry + (_rowH - iconH) / 2; if (iy < ry) iy = ry;
    view.drawGlyph(iconFont(), 2, iy, ic, col);
    tx = 2 + iconH + 3;
  }

  int ty = (_rowH - view.fontHeight(fontBody())) / 2; if (ty < 0) ty = 0;
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

  int availW = cw - tx - rightW; if (availW < 0) availW = 0;
  const char* lbl = _model->label(i);
  if (sel) _marquee.draw(view, fontBody(), tx, ry, availW, _rowH, lbl, col, now);
  else     view.drawTextEllipsized(fontBody(), tx, ry + ty, availW, lbl, col);
}

void ListMenu::draw(Canvas& c, int x, int y, int w, int h) {
  if (!_model) return;
  int n = _model->count();
  if (n > 0 && _selected >= n) _selected = n - 1;
  if (_selected != _lastSel) { _marquee.reset(); _lastSel = _selected; }

  uint32_t now = c.now();
  Canvas view = c.region(x, y, w, h);

  // Pixel scroll that keeps the selected row visible; the header scrolls too.
  int bodyTop = _headerH + (_header && _headerH > 0 ? 2 : 0);   // gap below the header
  int contentH = bodyTop + n * _rowH;
  int selTop = bodyTop + _selected * _rowH;
  int selBot = selTop + _rowH;
  if (selBot > _scrollPx + h) _scrollPx = selBot - h;
  if (selTop < _scrollPx) _scrollPx = selTop;
  if (_selected == 0) _scrollPx = 0;       // first row: scroll all the way up to reveal the header
  int maxScroll = contentH > h ? contentH - h : 0;
  if (_scrollPx > maxScroll) _scrollPx = maxScroll;
  if (_scrollPx < 0) _scrollPx = 0;

  bool scrollbar = contentH > h;
  int cw = scrollbar ? w - 4 : w;

  if (_header && _headerH > 0) {
    int hy = -_scrollPx;
    if (hy + _headerH > 0 && hy < h) _header->draw(view, 0, hy, cw, _headerH);
  }
  for (int i = 0; i < n; i++) {
    int ry = bodyTop + i * _rowH - _scrollPx;
    if (ry + _rowH <= 0 || ry >= h) continue;
    drawRow(view, i, ry, cw, now);
  }
  if (scrollbar) {
    int thumbH = h * h / contentH; if (thumbH < 3) thumbH = 3;
    int thumbY = maxScroll > 0 ? (h - thumbH) * _scrollPx / maxScroll : 0;
    view.fillRect(w - 2, thumbY, 2, thumbH, DisplayDriver::LIGHT);
  }
}

}  // namespace mishmesh
