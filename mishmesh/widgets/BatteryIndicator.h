#pragma once

#include <stdint.h>

namespace mishmesh {

class Canvas;

// The one battery readout: a drawn gauge (outline + nub + fill) or "87%" text,
// per the system-wide UiPrefs battery-display setting. StatusBar and the Home
// face both render through this so the setting applies everywhere at once.
class BatteryIndicator {
  uint16_t _mv = 0;
public:
  void setMillivolts(uint16_t mv) { _mv = mv; }
  // Draws with the right edge at xRight, vertically centered in [0, rowH).
  // Returns the width consumed so callers can lay out to its left.
  int drawRightAligned(Canvas& c, int xRight, int rowH);
  // Width drawRightAligned would consume, for layout done before drawing.
  int measureWidth(Canvas& c) const;
};

}  // namespace mishmesh
