#pragma once

#include <mishmesh/widgets/Widget.h>
#include <helpers/ui/DisplayDriver.h>
#include <mishmesh/core/Canvas.h>

struct mf_font_s;

namespace mishmesh {

// Single-line text in a box: font, color, alignment, ellipsis on overflow, and
// an optional leading icon glyph. The one place that owns truncation.
class Label : public Widget {
  const char*           _text;
  const mf_font_s*      _font;
  DisplayDriver::Color  _color;
  TextAlign             _align;
  uint16_t              _icon;     // iconFont() codepoint, 0 = none
public:
  Label();
  void set(const char* text, const mf_font_s* font,
           DisplayDriver::Color color = DisplayDriver::LIGHT,
           TextAlign align = TextAlign::Left);
  void setIcon(uint16_t icon) { _icon = icon; }
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
