#include <mishmesh/widgets/BatteryIndicator.h>
#include <mishmesh/widgets/StatusBar.h>   // batteryPercent()
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const int GAUGE_W = 14;   // 12 body + 2 nub
static const int GAUGE_H = 7;

// One decimal keeps the top-bar string short (e.g. "3.9V").
static void fmtVolts(char* out, uint16_t cap, uint16_t mv) {
  snprintf(out, cap, "%u.%uV", (unsigned)(mv / 1000), (unsigned)((mv % 1000) / 100));
}

int BatteryIndicator::measureWidth(Canvas& c) const {
  UiPrefs::BattMode m = uiPrefs().battMode();
  if (m == UiPrefs::BattMode::Gauge) return GAUGE_W;
  char txt[8];
  if (m == UiPrefs::BattMode::Voltage) fmtVolts(txt, sizeof(txt), _mv);
  else snprintf(txt, sizeof(txt), "%d%%", batteryPercent(_mv));
  return c.textWidth(fontBody(), txt);
}

int BatteryIndicator::drawRightAligned(Canvas& c, int xRight, int rowH) {
  int pct = batteryPercent(_mv);
  UiPrefs::BattMode m = uiPrefs().battMode();
  if (m != UiPrefs::BattMode::Gauge) {
    char txt[8];
    if (m == UiPrefs::BattMode::Voltage) fmtVolts(txt, sizeof(txt), _mv);
    else snprintf(txt, sizeof(txt), "%d%%", pct);
    int ty = (rowH - c.fontHeight(fontBody())) / 2;
    if (ty < 0) ty = 0;
    c.drawText(fontBody(), xRight, ty, txt, DisplayDriver::LIGHT, TextAlign::Right);
    return c.textWidth(fontBody(), txt);
  }
  int x = xRight - GAUGE_W;
  int y = (rowH - GAUGE_H) / 2;
  if (y < 0) y = 0;
  c.drawRect(x, y, 12, GAUGE_H, DisplayDriver::LIGHT);
  c.fillRect(x + 12, y + 2, 2, 3, DisplayDriver::LIGHT);
  int fill = pct * 8 / 100;               // 0..8 px inside the body
  if (fill > 0) c.fillRect(x + 2, y + 2, fill, 3, DisplayDriver::LIGHT);
  return GAUGE_W;
}

}  // namespace mishmesh
