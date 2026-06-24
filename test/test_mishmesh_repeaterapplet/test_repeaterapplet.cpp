#include <gtest/gtest.h>
#include <mishmesh/applets/RepeaterApplet.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct FakeTgt : RadioStagingTarget {
  RadioConfig cfg{915.8f, 250.0f, 10, 5, 20, false};
  const RadioConfig& stagedConfig() const override { return cfg; }
  void stageFrequency(float f) override { cfg.freqMhz = f; }
  void stageBandwidth(float) override {}
  void stageSf(uint8_t) override {}
  void stageCr(uint8_t) override {}
  void stageTxPower(int) override {}
  void stagePreset(int) override {}
  void stageRepeater(bool on) override { cfg.repeater = on; }
};
struct FakeApp : AppServices {
  float saved = 0.0f;
  uint16_t batt = 0;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return batt; }
  uint32_t epochSeconds() const override { return 0; }
  int repeatFreqCount() const override { return 3; }
  float repeatFreqMhz(int i) const override {
    static const float F[3] = {433.0f, 869.495f, 918.0f}; return F[i];
  }
  float savedRepeatFreq() const override { return saved; }
  void setSavedRepeatFreq(float f) override { saved = f; }
};
AppletContext ctxWith(AppServices* a, RadioStagingTarget* t) {
  AppletContext c; c.app = a;
  repeaterApplet().configure(t);
  return c;
}
}  // namespace

TEST(RepeaterApplet, ListHasOffPlusAllowedFreqs) {
  FakeApp app; FakeTgt tgt;
  AppletContext ctx = ctxWith(&app, &tgt);
  RepeaterApplet& a = repeaterApplet();
  a.onStart(ctx);
  EXPECT_EQ(4, a.count());              // Off + 3 frequencies
  EXPECT_STREQ("Off", a.label(0));
  EXPECT_TRUE(a.radioOn(0));            // repeat off -> Off selected
}

TEST(RepeaterApplet, SelectFrequencyEnablesAndRemembers) {
  FakeApp app; FakeTgt tgt;             // start 915.8, repeat off
  AppletContext ctx = ctxWith(&app, &tgt);
  RepeaterApplet& a = repeaterApplet();
  a.onStart(ctx);
  a.selectRowForTest(3);               // row 3 = 918.0
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_TRUE(tgt.cfg.repeater);
  EXPECT_FLOAT_EQ(918.0f, tgt.cfg.freqMhz);
  EXPECT_FLOAT_EQ(915.8f, app.saved);  // pre-repeat freq remembered
  EXPECT_TRUE(a.radioOn(3));
}

TEST(RepeaterApplet, SelectOffRestoresSavedFreq) {
  FakeApp app; FakeTgt tgt;
  app.saved = 915.8f;
  tgt.cfg = {869.495f, 250.0f, 10, 5, 20, true};   // currently repeating
  AppletContext ctx = ctxWith(&app, &tgt);
  RepeaterApplet& a = repeaterApplet();
  a.onStart(ctx);
  a.selectRowForTest(0);               // Off
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(tgt.cfg.repeater);
  EXPECT_FLOAT_EQ(915.8f, tgt.cfg.freqMhz);
  EXPECT_TRUE(a.radioOn(0));
}

// Regression: the status bar must reflect the live battery level. The applet used
// to never call setBattery(), so the gauge rendered empty regardless of charge.
TEST(RepeaterApplet, StatusBarShowsBatteryLevel) {
  FakeTgt tgt;
  size_t fills[2];
  for (int pass = 0; pass < 2; pass++) {
    FakeApp app; app.batt = pass == 0 ? 0 : 4200;   // empty vs full
    AppletContext ctx = ctxWith(&app, &tgt);
    RepeaterApplet& a = repeaterApplet();
    a.onStart(ctx);
    FakeDisplayDriver d(160, 80);
    Canvas c(&d, 0);
    a.onRender(c);
    fills[pass] = d.fills.size();
  }
  EXPECT_NE(fills[0], fills[1]);   // battery level now affects what is drawn
}

// Regression: the description + options scroll as one unit, so the list is handed
// the whole body (below the status bar), not just the strip left under a static
// caption. A longer paragraph used to leave under one row for the list.
TEST(RepeaterApplet, ListGetsWholeBodyHeight) {
  FakeApp app; FakeTgt tgt;             // repeater off -> row 0 (Off) selected
  AppletContext ctx = ctxWith(&app, &tgt);
  RepeaterApplet& a = repeaterApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  EXPECT_GE(a.bodyHeightForTest(), 4 * a.rowHeightForTest());   // room for the whole content
}

// Regression for "when Off is selected, we can't see the list": at the top of the
// scroll (Off selected), the description header must still leave room for at least
// two option rows below it. The old 4-line paragraph filled the screen and hid
// every frequency; the short one does not.
TEST(RepeaterApplet, OffLeavesRoomForOptionsBelowCaption) {
  FakeApp app; FakeTgt tgt;
  AppletContext ctx = ctxWith(&app, &tgt);
  RepeaterApplet& a = repeaterApplet();
  a.onStart(ctx);                       // Off (row 0) selected -> scroll at top
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  int belowCaption = a.bodyHeightForTest() - a.captionHeightForTest() - 2;
  EXPECT_GE(belowCaption, 2 * a.rowHeightForTest());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
