#pragma once

#include <helpers/ui/MomentaryButton.h>
#include <mishmesh/core/InputSource.h>
#include <mishmesh/core/InputMapping.h>

namespace mishmesh {

// One button whose click/long/double/triple gestures map to events. `reverse`
// and `pulldownup` describe the wiring (active-low pull-up boards pass reverse=true).
class ButtonGestureSource : public InputSource {
  MomentaryButton _btn;
  GestureMap _map;
public:
  ButtonGestureSource(int8_t pin, const GestureMap& map, int long_press_ms = 1000,
                      bool reverse = false, bool pulldownup = false)
      : _btn(pin, long_press_ms, reverse, pulldownup, /*multiclick*/true), _map(map) {}
  void begin() { _btn.begin(); }
  bool poll(InputReport& out) override;
};

// A 5-way joystick / D-pad; each direction's click maps to an event. Directions
// are single-click only (multiclick off) for snappy navigation.
class DirectionalSource : public InputSource {
  MomentaryButton _up, _down, _left, _right, _press;
  DirectionalMap _map;
public:
  DirectionalSource(int8_t up, int8_t down, int8_t left, int8_t right, int8_t press,
                    const DirectionalMap& map = DirectionalMap(), int long_press_ms = 1000,
                    bool reverse = false, bool pulldownup = false)
      : _up(up, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _down(down, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _left(left, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _right(right, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _press(press, long_press_ms, reverse, pulldownup, /*multiclick*/false), _map(map) {}
  void begin() {
    _up.begin(); _down.begin(); _left.begin(); _right.begin(); _press.begin();
  }
  bool poll(InputReport& out) override;
};

}  // namespace mishmesh
