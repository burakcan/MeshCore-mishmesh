#include <gtest/gtest.h>
#include <mishmesh/applets/settings/DevResetOnboardingPanel.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  int resetCalls = 0;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
  void resetOnboarding() override { resetCalls++; }
};
}  // namespace

TEST(DevResetOnboardingPanel, ConfirmCallsResetOnboarding) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  auto& p = devResetOnboardingSettings();
  p.begin(ctx); p.onShow();
  FakeDisplayDriver d; Canvas c(&d);
  p.renderBody(c, 0, 0, 128, 64);
  p.onInput(InputEvent::NavRight);   // dialog defaults to Confirm; ensure on Confirm
  p.onInput(InputEvent::Select);     // confirm
  EXPECT_EQ(1, app.resetCalls);
}

TEST(DevResetOnboardingPanel, CancelDoesNotReset) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  auto& p = devResetOnboardingSettings();
  p.begin(ctx); p.onShow();
  bool bubbled = p.onInput(InputEvent::Back);   // cancel bubbles to pop the detail
  EXPECT_FALSE(bubbled);
  EXPECT_EQ(0, app.resetCalls);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
