#pragma once

#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

// Horizontal tabs. NavLeft/NavRight move the selection (clamped). Tab labels
// point to caller-owned (static) storage; icons are iconFont() codepoints.
class TabBar : public Widget {
  static const int MAX_TABS = 6;
  const char* _labels[MAX_TABS];
  uint16_t    _icons[MAX_TABS];
  int         _count;
  int         _sel;
public:
  TabBar() : _count(0), _sel(0) {}
  void clear() { _count = 0; _sel = 0; }
  void addTab(const char* label, uint16_t icon = 0) {
    if (_count < MAX_TABS) { _labels[_count] = label ? label : ""; _icons[_count] = icon; _count++; }
  }
  int count() const { return _count; }
  int selected() const { return _sel; }
  void setSelected(int i) { if (i >= 0 && i < _count) _sel = i; }
  bool onInput(InputEvent ev) override;
  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
