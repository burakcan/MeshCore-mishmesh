#include <gtest/gtest.h>
#include <mishmesh/widgets/Button.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

// Idle button: a 1px outline (drawRect) framing the box, no full-area fill.
TEST(Button, IdleDrawsOutlineNotFill) {
  FakeDisplayDriver d; Canvas c(&d, 0);
  Button b; b.set("Hi", 0); b.setFocused(false);
  b.draw(c, 4, 4, 60, 16);

  bool framed = false;
  for (auto& r : d.rects) if (r.x == 4 && r.y == 4 && r.w == 60 && r.h == 16) framed = true;
  EXPECT_TRUE(framed) << "idle button must draw its border via drawRect";

  for (auto& f : d.fills) {
    bool coversBox = f.x <= 4 && f.y <= 4 && f.x + f.w >= 64 && f.y + f.h >= 20;
    EXPECT_FALSE(coversBox) << "idle button must not fill its whole area";
  }
}

// Focused button: a solid fill over the whole box (inverted), no idle outline.
TEST(Button, FocusedDrawsSolidFill) {
  FakeDisplayDriver d; Canvas c(&d, 0);
  Button b; b.set("Hi", 0); b.setFocused(true);
  b.draw(c, 4, 4, 60, 16);

  bool filled = false;
  for (auto& f : d.fills) if (f.x == 4 && f.y == 4 && f.w == 60 && f.h == 16) filled = true;
  EXPECT_TRUE(filled) << "focused button must fill its whole area";
}

// An icon is rendered through the icon font (glyph fills land inside the box).
TEST(Button, DrawsIconWhenSet) {
  FakeDisplayDriver d; Canvas c(&d, 0);
  Button noIcon; noIcon.set("Hi", 0); noIcon.draw(c, 0, 0, 60, 16);
  size_t baseline = d.fills.size();

  FakeDisplayDriver d2; Canvas c2(&d2, 0);
  Button withIcon; withIcon.set("Hi", (uint16_t)Icon::Feather); withIcon.draw(c2, 0, 0, 60, 16);
  EXPECT_GT(d2.fills.size(), baseline) << "icon glyph should add fills vs. label-only";
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
