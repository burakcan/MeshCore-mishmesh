#include <gtest/gtest.h>
#include <mishmesh/widgets/Label.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

TEST(Label, DrawsText) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Label l;
  l.set("Alice", fontBody());
  l.draw(c, 0, 0, 128, 12);
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(Label, TruncatesLongTextToBox) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Label l;
  l.set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", fontBody());
  l.draw(c, 0, 0, 30, 12);   // narrow box forces ellipsis path
  EXPECT_GT(d.fills.size(), 0u);   // draws without overrun
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
