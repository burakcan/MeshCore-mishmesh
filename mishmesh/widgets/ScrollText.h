#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

// Reusable scrollable panel of plain text lines, with an optional header widget
// (e.g. a Card) that scrolls together with the lines. NavUp/Down scroll; a thin
// scrollbar appears when content overflows. Fixed storage, no heap.
class ScrollText : public Widget {
  static const int MAX_LINES = 16;
  static const int LINE_LEN = 40;
  char        _lines[MAX_LINES][LINE_LEN];
  int         _count;
  Widget*     _header;
  int         _headerH;
  int         _scrollPx;
  mutable int _lineH;       // from the last draw, for input scrolling
  mutable int _viewH;
public:
  ScrollText() : _count(0), _header(nullptr), _headerH(0), _scrollPx(0), _lineH(9), _viewH(0) {}
  void clear() { _count = 0; _scrollPx = 0; }
  void setHeader(Widget* hdr, int height) { _header = hdr; _headerH = height; }
  void addLine(const char* s);
  void addf(const char* fmt, ...);
  int  count() const { return _count; }
  bool onInput(InputEvent ev) override;     // NavUp/Down scroll; else false
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
