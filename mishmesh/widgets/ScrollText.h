#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

// Reusable scrollable panel of plain text lines, with an optional header widget
// (e.g. a Card) that scrolls together with the lines. NavUp/Down scroll; a thin
// scrollbar appears when content overflows. Fixed storage, no heap.
class ScrollText : public Widget {
  static const int MAX_LINES = 16;
public:
  static const int LINE_LEN = 64;
private:
  char        _lines[MAX_LINES][LINE_LEN];
  int         _count;
  Widget*     _header;
  int         _headerH;
  int         _scrollPx, _scrollTarget;   // animated offset eases toward the target
  bool        _animReady;   // false => snap on the next draw (opened/cleared)
  bool        _animating;
  bool        _wrap = false;    // true => word-wrap each line instead of ellipsizing
  mutable int _lineH;       // from the last draw, for input scrolling
  mutable int _viewH;
  mutable int _contentH = 0;    // total content height from the last draw (wrap-aware)
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
  // Word-wrap each line to the panel width instead of truncating with an ellipsis.
  // Opt-in (default off) so existing single-line callers are unchanged. For consoles
  // / long-form text where full readability matters more than one row per entry.
  void setWrap(bool on) { _wrap = on; }
  void addLine(const char* s);
  void addf(const char* fmt, ...);
  // Snap the view to the bottom on the next draw (e.g. after appending output).
  // draw() clamps the oversized target to the real max scroll.
  void scrollToEnd() { _scrollTarget = (1 << 20); _animReady = false; }
  int  count() const { return _count; }
  const char* lineForTest(int i) const { return (i >= 0 && i < _count) ? _lines[i] : ""; }
  bool needsAnimation() const { return _animating; }
  // True when the scroll position is at or past the bottom of the content.
  // Uses state cached by the last draw(); returns true before the first draw
  // (content fits in a zero-height view - no overshoot possible).
  bool atBottom() const {
    int maxScroll = _contentH > _viewH ? _contentH - _viewH : 0;
    return _scrollTarget >= maxScroll;
  }
  bool onInput(InputEvent ev) override;     // NavUp/Down scroll; else false
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
