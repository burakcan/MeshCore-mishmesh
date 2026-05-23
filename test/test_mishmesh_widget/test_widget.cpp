#include <gtest/gtest.h>
#include <mishmesh/widgets/Widget.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
class BoxWidget : public Widget {
public:
  int drawn = 0;
  void measure(int& w, int& h) const override { w = 20; h = 10; }
  void draw(Canvas& c, int x, int y, int w, int h) override {
    drawn++;
    c.fillRect(x, y, w, h, DisplayDriver::LIGHT);
  }
};
}  // namespace

TEST(Widget, DefaultMeasureIsZero) {
  struct Bare : public Widget {
    void draw(Canvas&, int, int, int, int) override {}
  } w;
  int ww = -1, hh = -1;
  w.measure(ww, hh);
  EXPECT_EQ(0, ww);
  EXPECT_EQ(0, hh);
}

TEST(Widget, DefaultOnInputDoesNotConsume) {
  BoxWidget w;
  EXPECT_FALSE(w.onInput(InputEvent::Select));
}

TEST(Widget, DrawForwardsToCanvas) {
  FakeDisplayDriver d;
  Canvas c(&d);
  BoxWidget w;
  w.draw(c, 5, 5, 10, 10);
  EXPECT_EQ(1, w.drawn);
  ASSERT_EQ(1u, d.fills.size());
  EXPECT_EQ(5, d.fills[0].x);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
