#include <mishmesh/core/Canvas.h>

namespace mishmesh {

// Intersect a local rect with (0,0,bw,bh); false if nothing remains visible.
static bool clipLocal(int& x, int& y, int& w, int& h, int bw, int bh) {
  if (w <= 0 || h <= 0) return false;
  int x2 = x + w;
  int y2 = y + h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x2 > bw) x2 = bw;
  if (y2 > bh) y2 = bh;
  w = x2 - x;
  h = y2 - y;
  return w > 0 && h > 0;
}

Canvas Canvas::region(int x, int y, int w, int h) const {
  int cw = w;
  int ch = h;
  if (x + cw > _w) cw = _w - x;
  if (y + ch > _h) ch = _h - y;
  if (cw < 0) cw = 0;
  if (ch < 0) ch = 0;
  return Canvas(_d, _ox + x, _oy + y, cw, ch, _now);
}

void Canvas::fillRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (!clipLocal(x, y, w, h, _w, _h)) return;
  if (_d) {
    _d->setColor(c);
    _d->fillRect(_ox + x, _oy + y, w, h);
  }
}

void Canvas::drawRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (!clipLocal(x, y, w, h, _w, _h)) return;
  if (_d) {
    _d->setColor(c);
    _d->drawRect(_ox + x, _oy + y, w, h);
  }
}

void Canvas::text(int x, int y, const char* str, DisplayDriver::Color c) {
  if (!_d || str == nullptr) return;
  if (x < 0 || y < 0 || x >= _w || y >= _h) return;
  _d->setColor(c);
  _d->setCursor(_ox + x, _oy + y);
  _d->print(str);
}

int Canvas::textWidth(const char* str) const {
  return (_d && str) ? _d->getTextWidth(str) : 0;
}

void Canvas::textRight(int x_right, int y, const char* str, DisplayDriver::Color c) {
  text(x_right - textWidth(str), y, str, c);
}

void Canvas::textCentered(int cx, int y, const char* str, DisplayDriver::Color c) {
  text(cx - textWidth(str) / 2, y, str, c);
}

}  // namespace mishmesh
