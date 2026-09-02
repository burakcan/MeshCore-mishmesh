#include <mishmesh/core/Metrics.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

// Above the 61px and 64px compact panels, below the 122px short edge shared by
// both e-ink Standard orientations - the only split that matters in practice.
static const int REGULAR_MIN_EDGE = 100;

bool isRegularCanvas(const Canvas& c) {
  const int shortEdge = c.width() < c.height() ? c.width() : c.height();
  return shortEdge >= REGULAR_MIN_EDGE;
}

bool isPortrait(const Canvas& c) { return c.height() > c.width(); }

int rowHeightBonus(const Canvas& c) { return isRegularCanvas(c) ? 4 : 0; }

int barHeight(const Canvas& c, int base) {
  return isRegularCanvas(c) ? base + 4 : base;
}

const Font* tierFont(const Canvas& c) {
  return c.height() >= REGULAR_MIN_EDGE ? fontBody() : fontCaption();
}

int numScaleCap(const Canvas& c) {
  return c.height() >= REGULAR_MIN_EDGE ? 2 : 1;
}

}  // namespace mishmesh
