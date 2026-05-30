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

// A 5-way joystick / D-pad. Each direction fires on the press edge; holding Up
// or Down auto-repeats (fast scroll). Bounce is absorbed by the host's input
// debounce, so the buttons here are read directly (isPressed), not via gestures.
class DirectionalSource : public InputSource {
  static const uint32_t REPEAT_DELAY_MS = 350;     // hold this long before repeats start
  static const uint32_t REPEAT_INTERVAL_MS = 90;   // then one step per this many ms
  static const uint32_t DEBOUNCE_MS = 25;          // a level must be stable this long to count
  MomentaryButton _up, _down, _left, _right, _press;
  DirectionalMap _map;
  bool     _wasPressed[5];   // up,down,left,right,press (debounced)
  uint32_t _nextRepeat[5];
  bool     _rawPressed[5];   // last raw read, for debouncing
  uint32_t _rawSince[5];     // when the raw read last changed
public:
  DirectionalSource(int8_t up, int8_t down, int8_t left, int8_t right, int8_t press,
                    const DirectionalMap& map = DirectionalMap(), int long_press_ms = 1000,
                    bool reverse = false, bool pulldownup = false)
      : _up(up, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _down(down, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _left(left, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _right(right, long_press_ms, reverse, pulldownup, /*multiclick*/false),
        _press(press, long_press_ms, reverse, pulldownup, /*multiclick*/false), _map(map) {
    for (int i = 0; i < 5; i++) { _wasPressed[i] = false; _nextRepeat[i] = 0; _rawPressed[i] = false; _rawSince[i] = 0; }
  }
  void begin() {
    _up.begin(); _down.begin(); _left.begin(); _right.begin(); _press.begin();
  }
  bool poll(InputReport& out) override;
};

}  // namespace mishmesh
