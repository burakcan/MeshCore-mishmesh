#include <mishmesh/widgets/StatusBar.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>

namespace mishmesh {

static const int BAR_H = 12;

int batteryPercent(uint16_t millivolts) {
  const int empty = 3300, full = 4200;
  if (millivolts <= empty) return 0;
  if (millivolts >= full) return 100;
  return (millivolts - empty) * 100 / (full - empty);
}

void StatusBar::measure(int& w, int& h) const {
  w = 0;        // stretches to its box
  h = BAR_H;
}

void StatusBar::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas bar = c.region(x, y, w, h);
  bar.fillRect(0, h - 1, w, 1, DisplayDriver::LIGHT);   // baseline divider
  bar.text(0, 0, _title, DisplayDriver::LIGHT);

  char pct[8];
  snprintf(pct, sizeof(pct), "%d%%", batteryPercent(_millivolts));
  bar.textRight(w, 0, pct, DisplayDriver::LIGHT);
}

}  // namespace mishmesh
