#include <gtest/gtest.h>
#include <mishmesh/applets/TimezonePickerApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/WorldClock.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 1782993600u; } // 2026-07 summer
};
int g_picked = -99;
void capture(void* ctx, int idx) { (void)ctx; g_picked = idx; }
}  // namespace

TEST(TimezonePickerApplet, PreselectsAndFiresCallbackOnSelect) {
  g_picked = -99;
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  auto& p = timezonePickerApplet();
  p.configure(3, capture, nullptr);
  p.onStart(ctx);
  EXPECT_EQ(3, p.selectedForTest());          // preselected the configured index
  p.onInput(InputEvent::NavDown);             // move to index 4
  p.onInput(InputEvent::Select);
  EXPECT_EQ(4, g_picked);                      // callback got the highlighted index
}

TEST(TimezonePickerApplet, RendersRowsWithoutCrashing) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  auto& p = timezonePickerApplet();
  p.configure(-1, capture, nullptr);
  p.onStart(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  p.onRender(c);
  EXPECT_GT(d.fills.size(), 0u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
