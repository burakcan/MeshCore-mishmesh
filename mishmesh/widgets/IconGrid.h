#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>
#include <mishmesh/widgets/ListMenu.h>   // ListModel lives with ListMenu
#include <mishmesh/widgets/Marquee.h>

namespace mishmesh {

class Canvas;

// App-drawer list over a flat ListModel: one row per entry - a rounded icon box
// around icon() (black interior + light glyph, an "app tile"), label() to its
// right, value() as a right-edge badge. The selection is a rounded highlight
// that eases between rows (content inverts as it glides, same idiom as
// ListMenu); the focused label marquees if it overflows. Vertical scroll eases
// too. Any Nav step moves the selection by one (wrapping); Select is left for
// the owning applet.
class IconGrid : public Widget {
  const ListModel* _model;
  int _rowH;
  int _sel, _lastSel;
  // Scroll offset and highlight top both ease toward their targets so movement
  // glides instead of snapping.
  int _scrollPx, _scrollTarget;
  int _barY;            // animated highlight top, in content coords
  bool _animReady;      // false => snap to target on the next draw (open / wrap)
  bool _animating;
  Marquee _marquee;     // focused label, when it overflows its row

  void drawRowContent(Canvas& view, int i, int ry, int cw, DisplayDriver::Color col, uint32_t now);
public:
  static const int TICK_MS = 33;   // ~30 fps while animating

  IconGrid()
      : _model(nullptr), _rowH(20), _sel(0), _lastSel(-1),
        _scrollPx(0), _scrollTarget(0), _barY(0),
        _animReady(false), _animating(false) {}

  void setModel(const ListModel* m);   // resets selection only when the model changes
  void setRowHeight(int h) { if (h > 0) _rowH = h; }
  int  selected() const { return _sel; }
  bool needsAnimation() const { return _animating || _marquee.active(); }

  bool onInput(InputEvent ev) override;
  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
