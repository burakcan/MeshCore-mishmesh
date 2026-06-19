#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/widgets/BatteryIndicator.h>
#include <mishmesh/widgets/StatusBar.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

TEST(BatteryIndicator, GaugeModeDrawsOutlineNubAndFill) {
  uiPrefs().resetForTest();          // battShowPercent() == false -> gauge
  FakeDisplayDriver d;
  Canvas c(&d);
  BatteryIndicator b;
  b.setMillivolts(4200);             // 100% -> full 8px fill
  int w = b.drawRightAligned(c, 128, 12);
  EXPECT_EQ(14, w);
  ASSERT_EQ(1u, d.rects.size());     // the 12x7 outline
  EXPECT_EQ(12, d.rects[0].w);
  EXPECT_EQ(7, d.rects[0].h);
  EXPECT_EQ(2u, d.fills.size());     // nub + fill
}

TEST(BatteryIndicator, GaugeFillScalesWithCharge) {
  uiPrefs().resetForTest();
  FakeDisplayDriver d;
  Canvas c(&d);
  BatteryIndicator b;
  b.setMillivolts(3300);             // 0% -> no fill rect
  b.drawRightAligned(c, 128, 12);
  EXPECT_EQ(1u, d.fills.size());     // nub only
}

TEST(BatteryIndicator, PercentModeDrawsTextNotGauge) {
  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);
  uiPrefs().setBattShowPercent(true);
  FakeDisplayDriver d;
  Canvas c(&d);
  BatteryIndicator b;
  b.setMillivolts(4000);
  int w = b.drawRightAligned(c, 128, 12);
  EXPECT_TRUE(d.rects.empty());      // no gauge outline
  EXPECT_GT(w, 0);
  EXPECT_FALSE(d.fills.empty());     // glyph pixels rasterised as fills
}

TEST(StatusBar, RendersThroughIndicator) {
  uiPrefs().resetForTest();          // gauge mode
  FakeDisplayDriver d;
  Canvas c(&d);
  StatusBar bar;
  bar.setTitle("alice");
  bar.setBattery(3900);
  bar.draw(c, 0, 0, 128, 12);
  ASSERT_EQ(1u, d.rects.size());     // gauge outline present in the bar
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
