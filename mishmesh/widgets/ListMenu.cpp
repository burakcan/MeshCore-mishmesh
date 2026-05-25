#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

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
  if (!_model) return;
  int n = _model->count();
  int rows = h / _rowH;
  if (rows < 1) rows = 1;
  int top = firstVisibleRow(h);

  Canvas view = c.region(x, y, w, h);
  int ty = (_rowH - view.fontHeight(fontBody())) / 2;
  if (ty < 0) ty = 0;
  for (int i = top; i < n && i < top + rows; i++) {
    int ry = (i - top) * _rowH;
    bool sel = (i == _selected);
    DisplayDriver::Color col = sel ? DisplayDriver::DARK : DisplayDriver::LIGHT;
    if (sel) view.fillRect(0, ry, w, _rowH, DisplayDriver::LIGHT);

    int tx = 4;
    uint16_t ic = _model->icon(i);
    if (ic) {
      int iconH = view.fontHeight(iconFont());
      int iy = ry + (_rowH - iconH) / 2;
      if (iy < ry) iy = ry;
      view.drawGlyph(iconFont(), 2, iy, ic, col);
      tx = 2 + iconH + 3;
    }
    view.drawText(fontBody(), tx, ry + ty, _model->label(i), col);
  }
}

}  // namespace mishmesh
