#pragma once

#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;

// Base for reusable UI components: report a natural size, draw into a box,
// and optionally consume input.
class Widget {
public:
  virtual ~Widget() {}

  virtual void measure(int& w, int& h) const { w = 0; h = 0; }
  virtual void draw(Canvas& c, int x, int y, int w, int h) = 0;
  virtual bool onInput(InputEvent) { return false; }
};

}  // namespace mishmesh
