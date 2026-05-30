#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

namespace mishmesh {

void ScrollText::addLine(const char* s) {
  if (_count >= MAX_LINES) return;
  strncpy(_lines[_count], s ? s : "", LINE_LEN - 1);
  _lines[_count][LINE_LEN - 1] = 0;
  _count++;
}

void ScrollText::addf(const char* fmt, ...) {
  if (_count >= MAX_LINES) return;
  va_list ap; va_start(ap, fmt);
  vsnprintf(_lines[_count], LINE_LEN, fmt, ap);
  va_end(ap);
  _count++;
}

bool ScrollText::onInput(InputEvent ev) {
  int bodyTop = _headerH + (_header && _headerH > 0 ? 2 : 0);
  int contentH = bodyTop + _count * _lineH;
  int maxScroll = contentH > _viewH ? contentH - _viewH : 0;
  // Move the target a line; draw() eases _scrollPx toward it.
  if (ev == InputEvent::NavDown) { _scrollTarget += _lineH; if (_scrollTarget > maxScroll) _scrollTarget = maxScroll; return true; }
  if (ev == InputEvent::NavUp)   { _scrollTarget -= _lineH; if (_scrollTarget < 0) _scrollTarget = 0; return true; }
  return false;
}

void ScrollText::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  int lh = view.lineHeight(fontBody());
  _lineH = lh; _viewH = h;

  int bodyTop = _headerH + (_header && _headerH > 0 ? 2 : 0);   // gap below the header
  int contentH = bodyTop + _count * lh;
  int maxScroll = contentH > h ? contentH - h : 0;
  if (_scrollTarget > maxScroll) _scrollTarget = maxScroll;
  if (_scrollTarget < 0) _scrollTarget = 0;
  if (!_animReady) { _scrollPx = _scrollTarget; _animReady = true; }   // opened: snap, don't slide in
  else { int minStep = lh / 2; if (minStep < 3) minStep = 3; _scrollPx = approach(_scrollPx, _scrollTarget, minStep); }
  _animating = (_scrollPx != _scrollTarget);
  bool scroll = contentH > h;
  int cw = scroll ? w - 4 : w;

  if (_header && _headerH > 0) {
    int hy = -_scrollPx;
    if (hy + _headerH > 0 && hy < h) _header->draw(view, 0, hy, cw, _headerH);  // reserve scrollbar gutter
  }
  for (int i = 0; i < _count; i++) {
    int ly = bodyTop + i * lh - _scrollPx;
    if (ly + lh <= 0 || ly >= h) continue;
    view.drawTextEllipsized(fontBody(), 0, ly, cw, _lines[i], DisplayDriver::LIGHT);
  }
  if (scroll) {
    int thumbH = h * h / contentH; if (thumbH < 3) thumbH = 3;
    int thumbY = (h - thumbH) * _scrollPx / maxScroll;
    view.fillRect(w - 2, thumbY, 2, thumbH, DisplayDriver::LIGHT);
  }
}

}  // namespace mishmesh
