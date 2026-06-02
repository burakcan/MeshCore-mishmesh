#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

TEST(Fonts, AccessorsResolve) {
  EXPECT_NE(nullptr, fontBody());
  EXPECT_NE(nullptr, fontSubtitle());
  EXPECT_NE(nullptr, fontNum());
  EXPECT_NE(nullptr, iconFont());
}

TEST(Text, WidthIsPositiveAndGrowsWithLength) {
  FakeDisplayDriver d;
  Canvas c(&d);
  int hi = c.textWidth(fontBody(), "Hi");
  int hello = c.textWidth(fontBody(), "Hello world");
  EXPECT_GT(hi, 0);
  EXPECT_GT(hello, hi);
}

TEST(Text, DrawEmitsPixels) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.drawText(fontBody(), 0, 0, "Hi", DisplayDriver::LIGHT);
  EXPECT_FALSE(d.fills.empty());   // glyphs drawn as pixel runs
}

TEST(Text, EmptyStringDrawsNothing) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.drawText(fontBody(), 0, 0, "", DisplayDriver::LIGHT);
  EXPECT_TRUE(d.fills.empty());
}

TEST(Text, WrappingProducesMultipleLines) {
  FakeDisplayDriver d;
  Canvas c(&d);
  const char* para = "The quick brown fox jumps over the lazy dog again and again";
  int lh = c.lineHeight(fontBody());
  ASSERT_GT(lh, 0);
  int endY = c.drawTextWrapped(fontBody(), 0, 0, 80, para, DisplayDriver::LIGHT);
  EXPECT_GE(endY, lh * 2);   // wrapped onto at least two lines
}

TEST(Text, GlyphRendersAnIcon) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.drawGlyph(iconFont(), 0, 0, (uint16_t)Icon::Home, DisplayDriver::LIGHT);
  EXPECT_FALSE(d.fills.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
