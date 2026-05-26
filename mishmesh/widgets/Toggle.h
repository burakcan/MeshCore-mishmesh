#pragma once

#include <mishmesh/widgets/Widget.h>
#include <helpers/ui/DisplayDriver.h>

namespace mishmesh {

// ON/OFF state pill. Single source of truth for on/off rendering. _fg is the
// pill colour (text on the ON pill is the opposite), so a row drawn on an
// inverted/selected background can pass DARK to stay legible.
class Toggle : public Widget {
  bool _on;
  DisplayDriver::Color _fg;
public:
  Toggle() : _on(false), _fg(DisplayDriver::LIGHT) {}
  void setOn(bool on) { _on = on; }
  bool isOn() const { return _on; }
  void setColor(DisplayDriver::Color fg) { _fg = fg; }
  static const char* stateText(bool on) { return on ? "ON" : "OFF"; }
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
