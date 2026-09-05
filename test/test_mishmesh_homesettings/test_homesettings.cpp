#include <gtest/gtest.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/settings/HomeSettingsPanel.h>
#include <mishmesh/applets/settings/DisplaySettingsPanel.h>
#include <mishmesh/core/AppletHost.h>
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
  uint8_t brightness = 2;                           // High (persisted)
  uint8_t previewed = 2;                            // last live-applied level
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
  uint8_t screenSleepIndex() const override { return idx; }
  void setScreenSleepIndex(uint8_t i) override { idx = i; }
  bool screenBrightnessSupported() const override { return true; }
  uint8_t screenBrightnessIndex() const override { return brightness; }
  void setScreenBrightnessIndex(uint8_t i) override { brightness = i; previewed = i; }
  void previewScreenBrightnessIndex(uint8_t i) override { previewed = i; }
};

}  // namespace

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

TEST(DisplaySettingsPanel, ScreenSleepStepperAppliesSelection) {
  prime();
  FakeSleepApp app;
  AppletContext ctx; ctx.app = &app;
  DisplaySettingsPanel& p = displaySettings();
  p.begin(ctx);
  EXPECT_FALSE(p.modalActive());
  // Row 0 = Screen sleep: the interface-size row exists only on a panel that can
  // magnify, and FakeDisplayDriver reports that it cannot.
  EXPECT_TRUE(p.onInput(InputEvent::Select));        // open stepper at idx 1 (30s)
  EXPECT_TRUE(p.modalActive());
  EXPECT_TRUE(p.onInput(InputEvent::NavRight));       // 30s -> 1m (idx 2)
  EXPECT_TRUE(p.onInput(InputEvent::Select));         // confirm
  EXPECT_FALSE(p.modalActive());
  EXPECT_EQ(2, app.idx);
}


TEST(DisplaySettingsPanel, ScreenBrightnessStepperAppliesSelection) {
  prime();
  FakeSleepApp app;
  AppletContext ctx; ctx.app = &app;
  DisplaySettingsPanel& p = displaySettings();
  p.begin(ctx);
  // Rows here are Screen sleep, Return to home, Screen brightness.
  EXPECT_TRUE(p.onInput(InputEvent::NavDown));
  EXPECT_TRUE(p.onInput(InputEvent::NavDown));
  EXPECT_TRUE(p.onInput(InputEvent::Select));         // open at High (idx 2)
  EXPECT_TRUE(p.modalActive());
  EXPECT_TRUE(p.onInput(InputEvent::NavLeft));        // High -> Medium
  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_FALSE(p.modalActive());
  EXPECT_EQ(1, app.brightness);
}

TEST(DisplaySettingsPanel, ScreenBrightnessPreviewsWhileStepping) {
  prime();
  FakeSleepApp app;
  AppletContext ctx; ctx.app = &app;
  DisplaySettingsPanel& p = displaySettings();
  p.begin(ctx);
  EXPECT_TRUE(p.onInput(InputEvent::NavDown));
  EXPECT_TRUE(p.onInput(InputEvent::NavDown));
  EXPECT_TRUE(p.onInput(InputEvent::Select));         // open at High (idx 2)
  EXPECT_TRUE(p.onInput(InputEvent::NavLeft));        // -> Medium (idx 1)
  EXPECT_EQ(1, app.previewed);                        // applied live...
  EXPECT_EQ(2, app.brightness);                       // ...but not persisted yet
  EXPECT_TRUE(p.onInput(InputEvent::NavLeft));        // -> Low (idx 0)
  EXPECT_EQ(0, app.previewed);
  EXPECT_TRUE(p.onInput(InputEvent::Cancel));         // bail out
  EXPECT_FALSE(p.modalActive());
  EXPECT_EQ(2, app.brightness);                       // still the saved value
  EXPECT_EQ(2, app.previewed);                        // display reverted to saved
}


// The interface-size row is offered only where the driver can magnify, and Select
// toggles it rather than opening a stepper for a choice of two.
TEST(DisplaySettingsPanel, InterfaceSizeRowOnlyOnPanelsThatMagnify) {
  prime();
  FakeSleepApp app;
  FakeDisplayDriver plain;                 // supportsUiScale() == false
  AppletHost host(&plain, AppletContext{});
  AppletContext ctx; ctx.app = &app; ctx.host = &host;
  DisplaySettingsPanel& p = displaySettings();
  p.begin(ctx);
  EXPECT_STREQ("Screen sleep", p.rowLabelForTest(0));

  FakeDisplayDriver big;
  big.uiScalable = true;
  AppletHost host2(&big, AppletContext{});
  AppletContext ctx2; ctx2.app = &app; ctx2.host = &host2;
  p.begin(ctx2);
  EXPECT_STREQ("Interface size", p.rowLabelForTest(0));
  EXPECT_STREQ("Large", p.rowValueForTest(0));   // Large is the default

  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_STREQ("Standard", p.rowValueForTest(0));
  EXPECT_EQ(1, uiPrefs().uiScale());
  EXPECT_EQ(1, big.uiScale);               // and the driver was actually told
  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_STREQ("Large", p.rowValueForTest(0));
  EXPECT_EQ(2, uiPrefs().uiScale());
}

// Orientation is its own capability: a panel may magnify without turning, and
// the row opens a pick-one list rather than cycling four values in place.
TEST(DisplaySettingsPanel, OrientationRowsOnlyWhereTheDriverCanTurn) {
  prime();
  FakeSleepApp app;
  FakeDisplayDriver fixed;
  fixed.uiScalable = true;                 // magnifies, but cannot turn
  AppletHost host(&fixed, AppletContext{});
  AppletContext ctx; ctx.app = &app; ctx.host = &host;
  DisplaySettingsPanel& p = displaySettings();
  p.begin(ctx);
  EXPECT_STREQ("Screen sleep", p.rowLabelForTest(1));   // no Orientation, no Controls

  FakeDisplayDriver turns;
  turns.uiScalable = true;
  turns.orientable = true;
  AppletHost host2(&turns, AppletContext{});
  AppletContext ctx2; ctx2.app = &app; ctx2.host = &host2;
  p.begin(ctx2);
  EXPECT_STREQ("Orientation", p.rowLabelForTest(1));
  EXPECT_STREQ("Landscape", p.rowValueForTest(1));
  EXPECT_STREQ("Controls", p.rowLabelForTest(2));
  EXPECT_STREQ("Auto", p.rowValueForTest(2));
}

TEST(UiPrefsRotation, ControlsFollowTheScreenUntilOverridden) {
  prime();
  EXPECT_EQ(int(UiPrefs::INPUT_AUTO), uiPrefs().inputRotation());
  uiPrefs().setRotation(1);
  EXPECT_EQ(1, uiPrefs().effectiveInputRotation());   // auto tracks the panel
  uiPrefs().setRotation(3);
  EXPECT_EQ(3, uiPrefs().effectiveInputRotation());

  uiPrefs().setInputRotation(0);                      // pin the controls
  EXPECT_EQ(0, uiPrefs().effectiveInputRotation());
  uiPrefs().setRotation(2);
  EXPECT_EQ(0, uiPrefs().effectiveInputRotation());   // and the screen no longer moves them

  uiPrefs().setInputRotation(UiPrefs::INPUT_AUTO);
  EXPECT_EQ(2, uiPrefs().effectiveInputRotation());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
