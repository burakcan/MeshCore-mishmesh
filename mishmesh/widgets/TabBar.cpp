#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/BatteryIndicator.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

bool TabBar::onInput(InputEvent ev) {
  if (_count <= 1) return false;
  if (ev == InputEvent::NavRight) { _sel = (_sel + 1) % _count; return true; }            // wraps last -> first
  if (ev == InputEvent::NavLeft)  { _sel = (_sel - 1 + _count) % _count; return true; }   // wraps first -> last
  return false;
}

void TabBar::measure(int& w, int& h) const {
  w = 0;
  h = 13;
}

static const int DECO_GAP = 4;   // gap between the tab strip and the floating decoration

int TabBar::decoWidthForTest(Canvas& c) const {
  if (_battery) {
    BatteryIndicator b;
    b.setMillivolts(_batteryMv);
    return b.measureWidth(c) + DECO_GAP;
  }
  return _deco[0] ? c.textWidth(fontBody(), _deco) + DECO_GAP : 0;
}

// Tabs are icon-only when collapsed and icon+label when active. The strip is
// laid out at its natural width and scrolled so the (wider) active tab stays
// centred; tabs scrolled past an edge clip, which hints there are more. The
// decoration floats at the right edge, outside the scrolled/clipped strip.
void TabBar::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);

  const int ICON_W = 12, PAD = 4, GAP = 3;
  int iconH = view.fontHeight(iconFont());
  int bodyH = view.fontHeight(fontBody());

  // Reserve the decoration's width on the right; the strip uses what's left.
  BatteryIndicator batt;
  batt.setMillivolts(_batteryMv);
  int decoW = _battery ? batt.measureWidth(view) + DECO_GAP
            : _deco[0] ? view.textWidth(fontBody(), _deco) + DECO_GAP : 0;
  int stripW = w - decoW; if (stripW < 0) stripW = 0;

  if (_count > 0) {
    Canvas strip = view.region(0, 0, stripW, h);   // clips tabs out of the decoration slot

    int tabW[MAX_TABS];
    int total = 0;
    for (int i = 0; i < _count; i++) {
      if (i == _sel) tabW[i] = PAD + ICON_W + GAP + strip.textWidth(fontBody(), _labels[i]) + PAD;
      else           tabW[i] = ICON_W + PAD + 2;
      total += tabW[i];
    }

    int offset;
    if (total <= stripW) {
      offset = 0;                           // left-align the strip when it fits
    } else {
      int activeX = 0;
      for (int i = 0; i < _sel; i++) activeX += tabW[i];
      offset = activeX + tabW[_sel] / 2 - stripW / 2;   // centre the active tab
      if (offset < 0) offset = 0;
      if (offset > total - stripW) offset = total - stripW;
    }

    int tx = 0;
    for (int i = 0; i < _count; i++) {
      int sx = tx - offset, tw = tabW[i];
      tx += tw;
      if (sx + tw <= 0 || sx >= stripW) continue;   // fully off-screen
      bool sel = (i == _sel);
      DisplayDriver::Color col = sel ? DisplayDriver::DARK : DisplayDriver::LIGHT;
      // Highlight runs the full bar height (minus the bottom border row) so the
      // centred 10px icon gets 1px of breathing room above and below it.
      if (sel) strip.fillRect(sx, 0, tw, h - 1, DisplayDriver::LIGHT);

      int iconX = sel ? sx + PAD : sx + (tw - ICON_W) / 2;
      strip.drawGlyph(iconFont(), iconX, (h - iconH) / 2, _icons[i], col);
      if (sel) {
        int lx = sx + PAD + ICON_W + GAP;
        strip.drawTextEllipsized(fontBody(), lx, (h - bodyH) / 2, sx + tw - PAD - lx, _labels[i], col);
      }
    }
  }

  // Floating decoration: right-aligned, drawn on the full-width view so it never
  // scrolls with the strip.
  if (_battery)
    batt.drawRightAligned(view, w, h - 1);
  else if (_deco[0])
    view.drawText(fontBody(), w, (h - bodyH) / 2, _deco, DisplayDriver::LIGHT, TextAlign::Right);

  view.fillRect(0, h - 1, w, 1, DisplayDriver::LIGHT);   // full-width baseline divider
}

}  // namespace mishmesh
