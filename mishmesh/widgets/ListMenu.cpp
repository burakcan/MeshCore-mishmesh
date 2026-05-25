#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/core/Canvas.h>

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

void ListMenu::draw(Canvas& c, int x, int y, int w, int h) {
  if (!_model) return;
  int n = _model->count();
  int rows = h / _rowH;
  if (rows < 1) rows = 1;

  // Derive the scroll window from the selection: keep it on screen, don't
  // overscroll past the last row.
  int top = _selected - rows + 1;
  if (top < 0) top = 0;
  if (top > n - rows) top = n - rows;
  if (top < 0) top = 0;

  Canvas view = c.region(x, y, w, h);
  for (int i = top; i < n && i < top + rows; i++) {
    int ry = (i - top) * _rowH;
    bool sel = (i == _selected);
    if (sel) view.fillRect(0, ry, w, _rowH, DisplayDriver::LIGHT);
    view.text(2, ry + 2, _model->label(i),
              sel ? DisplayDriver::DARK : DisplayDriver::LIGHT);
  }
}

}  // namespace mishmesh
