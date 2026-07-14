#include <gtest/gtest.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/settings/HomeSettingsPanel.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {

struct StubApplet : Applet {
  StubApplet() : Applet("stub") {}
  int onRender(Canvas&) override { return 1000; }
};

StubApplet a1, a2;
AppletRegistration r1{&a1, "Contacts", 0, Placement::AppMenu, 1, nullptr};
AppletRegistration r2{&a2, "Messages", 0, Placement::AppMenu, 0, nullptr};

void prime() {
  resetRegistry();
  registerApplet(&r1);
  registerApplet(&r2);
  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);
}

struct FakeSleepApp : AppServices {
  uint8_t idx = 1;                                  // 30s
  uint8_t brightness = 4;                           // Maximum
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
  uint8_t screenSleepIndex() const override { return idx; }
  void setScreenSleepIndex(uint8_t i) override { idx = i; }
  bool screenBrightnessSupported() const override { return true; }
  uint8_t screenBrightnessIndex() const override { return brightness; }
  void setScreenBrightnessIndex(uint8_t i) override { brightness = i; }
};

}  // namespace

TEST(HomeSettingsPanel, TogglesBatteryPercent) {
  prime();
  AppletContext ctx;
  HomeSettingsPanel& p = homeSettings();
  p.begin(ctx);
  EXPECT_STREQ("Home", p.title());
  EXPECT_FALSE(uiPrefs().battShowPercent());
  // Row 0 = battery toggle; selection starts there.
  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_TRUE(uiPrefs().battShowPercent());
  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_FALSE(uiPrefs().battShowPercent());
}

TEST(QuickActionPickerPanel, PicksAnAppletForASlot) {
  prime();
  AppletContext ctx;
  QuickActionPickerPanel& pick = quickActionPicker();
  pick.setSlot(UiPrefs::SLOT_RIGHT);
  pick.begin(ctx);
  // Rows are AppMenu registrations sorted by order: [Messages, Contacts].
  EXPECT_TRUE(pick.onInput(InputEvent::NavDown));   // move to "Contacts"
  EXPECT_TRUE(pick.onInput(InputEvent::Select));
  EXPECT_STREQ("Contacts", uiPrefs().quickActionLabel(UiPrefs::SLOT_RIGHT));
  EXPECT_EQ(&a1, uiPrefs().quickAction(UiPrefs::SLOT_RIGHT)->applet);
}

TEST(HomeSettingsPanel, RendersWithoutHost) {
  prime();
  AppletContext ctx;
  HomeSettingsPanel& p = homeSettings();
  p.begin(ctx);
  FakeDisplayDriver d;
  Canvas c(&d);
  p.renderBody(c, 0, 13, 128, 51);
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(HomeSettingsPanel, ScreenSleepStepperAppliesSelection) {
  prime();
  FakeSleepApp app;
  AppletContext ctx; ctx.app = &app;
  HomeSettingsPanel& p = homeSettings();
  p.begin(ctx);
  EXPECT_FALSE(p.modalActive());
  // Row 0 = battery, row 1 = Screen sleep.
  EXPECT_TRUE(p.onInput(InputEvent::NavDown));       // -> row 1
  EXPECT_TRUE(p.onInput(InputEvent::Select));        // open stepper at idx 1 (30s)
  EXPECT_TRUE(p.modalActive());
  EXPECT_TRUE(p.onInput(InputEvent::NavRight));       // 30s -> 1m (idx 2)
  EXPECT_TRUE(p.onInput(InputEvent::Select));         // confirm
  EXPECT_FALSE(p.modalActive());
  EXPECT_EQ(2, app.idx);
}


TEST(HomeSettingsPanel, ScreenBrightnessStepperAppliesSelection) {
  prime();
  FakeSleepApp app;
  AppletContext ctx; ctx.app = &app;
  HomeSettingsPanel& p = homeSettings();
  p.begin(ctx);
  // Brightness is the fifth row, after the two shortcut rows.
  for (int i = 0; i < 4; i++) EXPECT_TRUE(p.onInput(InputEvent::NavDown));
  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_TRUE(p.modalActive());
  EXPECT_TRUE(p.onInput(InputEvent::NavLeft));        // Maximum -> High
  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_FALSE(p.modalActive());
  EXPECT_EQ(3, app.brightness);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
