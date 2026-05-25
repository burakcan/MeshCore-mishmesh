#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

class Canvas;

// LiPo voltage -> 0..100%, clamped (coarse linear curve).
int batteryPercent(uint16_t millivolts);

class StatusBar : public Widget {
  const char* _title;
  uint16_t _millivolts;
public:
  StatusBar() : _title(""), _millivolts(0) {}

  void setTitle(const char* t) { _title = t ? t : ""; }
  void setBattery(uint16_t millivolts) { _millivolts = millivolts; }

  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
