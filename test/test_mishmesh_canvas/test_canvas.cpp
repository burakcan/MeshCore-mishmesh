#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

TEST(Canvas, ReportsDisplaySize) {
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  EXPECT_EQ(128, c.width());
  EXPECT_EQ(64, c.height());
}

TEST(Canvas, FillRectClipsToDisplayBounds) {
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  c.fillRect(120, 60, 20, 20, DisplayDriver::LIGHT);  // overruns right+bottom
  ASSERT_EQ(1u, d.fills.size());
  EXPECT_EQ(120, d.fills[0].x);
  EXPECT_EQ(60, d.fills[0].y);
  EXPECT_EQ(8, d.fills[0].w);   // 128-120
  EXPECT_EQ(4, d.fills[0].h);   // 64-60
  EXPECT_EQ(DisplayDriver::LIGHT, d.lastColor);
}

TEST(Canvas, RegionTranslatesAndClips) {
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  Canvas sub = c.region(10, 10, 30, 30);
  EXPECT_EQ(30, sub.width());
  EXPECT_EQ(30, sub.height());
  // Draw bigger than the region: clipped to 30x30, translated by the region origin.
  sub.fillRect(0, 0, 100, 100, DisplayDriver::LIGHT);
  ASSERT_EQ(1u, d.fills.size());
  EXPECT_EQ(10, d.fills[0].x);
  EXPECT_EQ(10, d.fills[0].y);
  EXPECT_EQ(30, d.fills[0].w);
  EXPECT_EQ(30, d.fills[0].h);
}

TEST(Canvas, RegionSizeClampedToParent) {
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  Canvas sub = c.region(100, 50, 100, 100);  // would extend past the display
  EXPECT_EQ(28, sub.width());   // 128-100
  EXPECT_EQ(14, sub.height());  // 64-50
}

TEST(Canvas, FullyOutOfBoundsRectIsDropped) {
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  c.fillRect(200, 200, 10, 10, DisplayDriver::LIGHT);
  EXPECT_TRUE(d.fills.empty());
}

TEST(Canvas, NowIsSettableAndReadable) {
  FakeDisplayDriver d;
  Canvas c(&d, 1234);
  EXPECT_EQ(1234u, c.now());
  c.setNow(5000);
  EXPECT_EQ(5000u, c.now());
}

TEST(CanvasEllipsis, ShortTextDrawsWhole) {
  FakeDisplayDriver d;
  mishmesh::Canvas c(&d);
  // Body font is compiled in the native env (fonts/*.c).
  c.drawTextEllipsized(mishmesh::fontBody(), 0, 0, 1000, "Hi", DisplayDriver::LIGHT);
  // No truncation marker expected; at least one pixel run drawn.
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(CanvasEllipsis, LongTextFitsWithinMaxWidth) {
  FakeDisplayDriver d;
  mishmesh::Canvas c(&d);
  const mf_font_s* f = mishmesh::fontBody();
  const char* longstr = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int full = c.textWidth(f, longstr);
  int maxw = full / 3;
  // Must not throw / overrun; drawing happens via fills.
  c.drawTextEllipsized(f, 0, 0, maxw, longstr, DisplayDriver::LIGHT);
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(CanvasStipple, FillsCheckerboard) {
  FakeDisplayDriver d;
  mishmesh::Canvas c(&d);
  c.fillStipple(0, 0, 4, 4, DisplayDriver::LIGHT);
  // 4x4 checkerboard => 8 set pixels, each a 1x1 fillRect.
  EXPECT_EQ(8u, d.fills.size());
  for (auto& r : d.fills) {
    EXPECT_EQ(1, r.w);
    EXPECT_EQ(1, r.h);
    EXPECT_EQ(0, (r.x + r.y) & 1);   // checkerboard parity
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
