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
  // Uses the content height cached by the last draw() (wrap-aware); scrolls a line at a time.
  int maxScroll = _contentH > _viewH ? _contentH - _viewH : 0;
  if (ev == InputEvent::NavDown) { _scrollTarget += _lineH; if (_scrollTarget > maxScroll) _scrollTarget = maxScroll; return true; }
  if (ev == InputEvent::NavUp)   { _scrollTarget -= _lineH; if (_scrollTarget < 0) _scrollTarget = 0; return true; }
  return false;
}

void ScrollText::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  const mf_font_s* font = fontBody();
  int lh = view.lineHeight(font);
  _lineH = lh; _viewH = h;

  int bodyTop = _headerH + (_header && _headerH > 0 ? 2 : 0);   // gap below the header

  // Per-line heights: one row when ellipsizing, the wrapped height when wrapping.
  // In wrap mode reserve the scrollbar gutter unconditionally so the wrap width
  // (and thus the measured heights) don't change when the bar appears/disappears.
  int cwWrap = w - 4;
  int lineHt[MAX_LINES];
  int contentH = bodyTop;
  for (int i = 0; i < _count; i++) {
    if (_wrap) {
      int mh = view.measureTextWrapped(font, cwWrap, _lines[i]);
      lineHt[i] = mh > 0 ? mh : lh;   // keep blank lines one row tall
    } else {
      lineHt[i] = lh;
    }
    contentH += lineHt[i];
  }
  int footerTop = contentH + (_footer && _footerH > 0 ? 2 : 0);   // gap above the footer
  if (_footer && _footerH > 0) contentH = footerTop + _footerH;
  _contentH = contentH;

  int maxScroll = contentH > h ? contentH - h : 0;
  if (_scrollTarget > maxScroll) _scrollTarget = maxScroll;
  if (_scrollTarget < 0) _scrollTarget = 0;
  if (!_animReady) { _scrollPx = _scrollTarget; _animReady = true; }   // opened: snap, don't slide in
  else { int minStep = lh / 2; if (minStep < 3) minStep = 3; _scrollPx = approach(_scrollPx, _scrollTarget, minStep); }
  _animating = (_scrollPx != _scrollTarget);
  bool scroll = contentH > h;
  int cw = _wrap ? cwWrap : (scroll ? w - 4 : w);

  if (_header && _headerH > 0) {
    int hy = -_scrollPx;
    if (hy + _headerH > 0 && hy < h) _header->draw(view, 0, hy, cw, _headerH);  // reserve scrollbar gutter
  }
  int ly = bodyTop - _scrollPx;
  for (int i = 0; i < _count; i++) {
    if (ly + lineHt[i] > 0 && ly < h) {
      if (_wrap) view.drawTextWrapped(font, 0, ly, cw, _lines[i], DisplayDriver::LIGHT);
      else       view.drawTextEllipsized(font, 0, ly, cw, _lines[i], DisplayDriver::LIGHT);
    }
    ly += lineHt[i];
  }
  if (_footer && _footerH > 0) {
    int fy = footerTop - _scrollPx;
    if (_footerDivider) {
      int dy = fy - 1;
      if (dy >= 0 && dy < h) view.fillRect(0, dy, cw, 1, DisplayDriver::LIGHT);
    }
    if (fy + _footerH > 0 && fy < h) _footer->draw(view, 0, fy, cw, _footerH);
  }
  if (scroll) {
    view.drawScrollbarThumb(w - 2, h, contentH, _scrollPx);
  }
}

}  // namespace mishmesh
