#include <gtest/gtest.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

TEST(TabBar, NavRightLeftMovesSelectionWrapping) {
  TabBar t;
  t.addTab("A"); t.addTab("B"); t.addTab("C");
  EXPECT_EQ(0, t.selected());
  EXPECT_TRUE(t.onInput(InputEvent::NavLeft));    // wraps first -> last
  EXPECT_EQ(2, t.selected());
  EXPECT_TRUE(t.onInput(InputEvent::NavRight));   // wraps last -> first
  EXPECT_EQ(0, t.selected());
  t.onInput(InputEvent::NavRight); t.onInput(InputEvent::NavRight);
  EXPECT_EQ(2, t.selected());           // moved to last
  EXPECT_TRUE(t.onInput(InputEvent::NavRight));   // wraps to first
  EXPECT_EQ(0, t.selected());
}

TEST(TabBar, SingleTabDoesNotConsumeNav) {
  TabBar t; t.addTab("only");
  EXPECT_FALSE(t.onInput(InputEvent::NavRight));   // nowhere to wrap to -> bubbles
  EXPECT_FALSE(t.onInput(InputEvent::NavLeft));
  EXPECT_EQ(0, t.selected());
}

TEST(TabBar, IgnoresNonHorizontalInput) {
  TabBar t; t.addTab("A"); t.addTab("B");
  EXPECT_FALSE(t.onInput(InputEvent::NavUp));
  EXPECT_FALSE(t.onInput(InputEvent::Select));
}

TEST(TabBar, DrawsHighlight) {
  FakeDisplayDriver d; Canvas c(&d);
  TabBar t; t.addTab("A"); t.addTab("B");
  t.draw(c, 0, 0, 128, 12);
  EXPECT_GT(d.fills.size(), 0u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
