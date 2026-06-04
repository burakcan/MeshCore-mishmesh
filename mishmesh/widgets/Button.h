#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

class Canvas;

// A bordered push-button: an optional left icon + centered label inside a 1px
// frame. Focused draws solid-filled (inverted) like the other focus idioms;
// unfocused draws as an outline. Stateless beyond its label/icon/focus, so one
// instance can render a whole row of buttons by re-setting it between draws.
class Button : public Widget {
  const char* _label;
  uint16_t    _icon;     // iconFont() codepoint, 0 = none
  bool        _focused;
public:
  Button() : _label(""), _icon(0), _focused(false) {}
  void set(const char* label, uint16_t icon) { _label = label ? label : ""; _icon = icon; }
  void setFocused(bool f) { _focused = f; }
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
