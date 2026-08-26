#include <mishmesh/widgets/GridView.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

static const int ICON_PX = 12;   // iconFont() glyph box

void GridView::setModel(const GridModel* m) {
  _model = m;
  _row = 0;
  _col = 0;
}

bool GridView::onInput(InputEvent ev) {
  if (!_model) return false;
  int rows = _model->rows(), cols = _model->cols();
  if (rows <= 0 || cols <= 0) return false;
  switch (ev) {
    case InputEvent::NavUp:    _row = (_row - 1 + rows) % rows; return true;
    case InputEvent::NavDown:  _row = (_row + 1) % rows;        return true;
    case InputEvent::NavLeft:  _col = (_col - 1 + cols) % cols; return true;
    case InputEvent::NavRight: _col = (_col + 1) % cols;        return true;
    default: return false;
  }
}

void GridView::measure(int& w, int& h) const { w = 0; h = 0; }

void GridView::draw(Canvas& c, int x, int y, int w, int h) {
  if (!_model) return;
  int rows = _model->rows(), cols = _model->cols();
  if (rows <= 0 || cols <= 0) return;
  int cw = w / cols, ch = h / rows;
  for (int r = 0; r < rows; r++) {
    for (int col = 0; col < cols; col++) {
      int cx = x + col * cw, cy = y + r * ch;
      bool sel = _focusVisible && (r == _row && col == _col);
      DisplayDriver::Color fg = DisplayDriver::LIGHT;
      if (sel) {
        c.fillRect(cx, cy, cw, ch, DisplayDriver::LIGHT);
        fg = DisplayDriver::DARK;
      }
      uint16_t icon = _model->cellIcon(r, col);
      if (icon) {
        c.drawGlyph(iconFont(), cx + (cw - ICON_PX) / 2,
                    cy + (ch - ICON_PX) / 2, icon, fg);
      } else {
        const char* lbl = _model->cellLabel(r, col);
        if (lbl && lbl[0]) {
          int ty = cy + (ch - c.fontHeight(fontBody())) / 2;
          c.drawTextEllipsized(fontBody(), cx + cw / 2, ty, cw - 2, lbl, fg, TextAlign::Center);
        }
      }
    }
  }
}

}  // namespace mishmesh
