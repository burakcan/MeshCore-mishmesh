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
  int         _scrollPx, _scrollTarget;   // animated offset eases toward the target
  bool        _animReady;   // false => snap on the next draw (opened/cleared)
  bool        _animating;
  mutable int _lineH;       // from the last draw, for input scrolling
  mutable int _viewH;
public:
  ScrollText() : _count(0), _header(nullptr), _headerH(0), _scrollPx(0), _scrollTarget(0),
                 _animReady(false), _animating(false), _lineH(9), _viewH(0) {}
  // keepScroll retains the current scroll offset (for live content refreshes
  // that shouldn't jump the user back to the top); default snaps to the top.
  void clear(bool keepScroll = false) {
    _count = 0;
    if (!keepScroll) { _scrollPx = _scrollTarget = 0; _animReady = false; }
  }
  void setHeader(Widget* hdr, int height) { _header = hdr; _headerH = height; }
  void addLine(const char* s);
  void addf(const char* fmt, ...);
  int  count() const { return _count; }
  const char* lineForTest(int i) const { return (i >= 0 && i < _count) ? _lines[i] : ""; }
  bool needsAnimation() const { return _animating; }
  bool onInput(InputEvent ev) override;     // NavUp/Down scroll; else false
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
