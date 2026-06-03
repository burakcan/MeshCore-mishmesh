#include <gtest/gtest.h>
#include <mishmesh/widgets/GridView.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

namespace {
// 2 rows x 3 cols test model.
struct TestModel : GridModel {
  int rows() const override { return 2; }
  int cols() const override { return 3; }
  const char* cellLabel(int r, int c) const override {
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d,%d", r, c);
    return buf;
  }
};
}

TEST(GridView, NavWrapsInBothAxes) {
  TestModel m;
  GridView g;
  g.setModel(&m);
  EXPECT_EQ(0, g.focusedRow());
  EXPECT_EQ(0, g.focusedCol());

  EXPECT_TRUE(g.onInput(InputEvent::NavLeft));   // col wraps 0 -> 2
  EXPECT_EQ(2, g.focusedCol());
  EXPECT_TRUE(g.onInput(InputEvent::NavRight));  // col wraps 2 -> 0
  EXPECT_EQ(0, g.focusedCol());
  EXPECT_TRUE(g.onInput(InputEvent::NavUp));     // row wraps 0 -> 1
  EXPECT_EQ(1, g.focusedRow());
  EXPECT_TRUE(g.onInput(InputEvent::NavDown));   // row wraps 1 -> 0
  EXPECT_EQ(0, g.focusedRow());
}

TEST(GridView, SetFocusAndNonNavBubbles) {
  TestModel m;
  GridView g;
  g.setModel(&m);
  g.setFocus(1, 2);
  EXPECT_EQ(1, g.focusedRow());
  EXPECT_EQ(2, g.focusedCol());
  EXPECT_FALSE(g.onInput(InputEvent::Select));   // Select is owner's job
  EXPECT_FALSE(g.onInput(InputEvent::Back));
}

TEST(GridView, NoModelDoesNotCrashOrConsume) {
  GridView g;
  EXPECT_FALSE(g.onInput(InputEvent::NavLeft));
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 52);                      // must be a no-op, no crash
  EXPECT_EQ(0u, d.fills.size());
}

TEST(GridView, DrawHighlightsFocusedCell) {
  TestModel m;
  GridView g;
  g.setModel(&m);
  g.setFocus(0, 0);
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 120, 40);                      // 3 cols -> cw=40, 2 rows -> ch=20
  ASSERT_GT(d.fills.size(), 0u);
  // The selected cell (0,0) is filled at its top-left.
  EXPECT_EQ(0, d.fills[0].x);
  EXPECT_EQ(0, d.fills[0].y);
  EXPECT_EQ(40, d.fills[0].w);
  EXPECT_EQ(20, d.fills[0].h);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
