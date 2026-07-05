#include <gtest/gtest.h>
#include <mishmesh/applets/OnboardingApplet.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
#include <string.h>

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  char name[32] = "OLD";
  RadioConfig radio{910.0f, 250.0f, 10, 5, 20, false};   // matches no preset
  bool gpsSup = true, gpsOn = false, autoSync = true;
  int tzCity = -1;
  char setName[32] = {0}; bool didSetRadio = false; RadioConfig setRadioCfg{};
  bool didSetGps = false, setGpsVal = false;
  int setTzVal = -99; bool didSetTz = false;
  bool didSetAuto = false, setAutoVal = true;
  bool didComplete = false;

  const char* nodeName() const override { return name; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 1782993600u; }
  bool setNodeName(const char* n) override { snprintf(setName, sizeof(setName), "%s", n); return true; }
  bool radioConfig(RadioConfig& out) const override { out = radio; return true; }
  void setRadioConfig(const RadioConfig& c) override { didSetRadio = true; setRadioCfg = c; }
  bool gpsSupported() const override { return gpsSup; }
  bool gpsEnabled() const override { return gpsOn; }
  void setGpsEnabled(bool on) override { didSetGps = true; setGpsVal = on; }
  int  tzCityIndex() const override { return tzCity; }
  void setTzCity(int i) override { didSetTz = true; setTzVal = i; }
  bool autoTimeSync() const override { return autoSync; }
  void setAutoTimeSync(bool on) override { didSetAuto = true; setAutoVal = on; }
  void markOnboardingComplete() override { didComplete = true; }
};

int g_doneCalls = 0;
void onDone(void*) { g_doneCalls++; }

// Advance one step. Name/GPS/Time: move focus onto the Next button then Select.
// Welcome/Done: Select. Radio region: Select picks the focused preset + auto-advances.
void advance(OnboardingApplet& w) {
  int br = w.buttonRowForTest();
  if (br < 0) { w.onInput(InputEvent::Select); return; }
  for (int i = 0; i < 40 && w.focusForTest() != br; i++) w.onInput(InputEvent::NavDown);
  w.onInput(InputEvent::Select);
}
void gotoStep(OnboardingApplet& w, int target) {
  for (int i = 0; i < 12 && w.stepForTest() < target; i++) advance(w);
}
}  // namespace

TEST(ShouldShowOnboarding, TruthTable) {
  EXPECT_FALSE(shouldShowOnboarding(2, true));
  EXPECT_FALSE(shouldShowOnboarding(2, false));
  EXPECT_TRUE (shouldShowOnboarding(1, false));
  EXPECT_TRUE (shouldShowOnboarding(1, true));
  EXPECT_TRUE (shouldShowOnboarding(0, true));
  EXPECT_FALSE(shouldShowOnboarding(0, false));
}

TEST(OnboardingApplet, GpsStepPresentWhenSupported) {
  FakeApp app; app.gpsSup = true;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(6, w.stepCountForTest());
}

TEST(OnboardingApplet, GpsStepHiddenWhenUnsupported) {
  FakeApp app; app.gpsSup = false;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(5, w.stepCountForTest());
}

TEST(OnboardingApplet, NextAdvancesBackReturns) {
  FakeApp app; AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(0, w.stepForTest());
  advance(w); EXPECT_EQ(1, w.stepForTest());   // Welcome -> Name
  advance(w); EXPECT_EQ(2, w.stepForTest());   // Name -> Region
  w.onInput(InputEvent::Back); EXPECT_EQ(1, w.stepForTest());
  w.onInput(InputEvent::Back); EXPECT_EQ(0, w.stepForTest());
  w.onInput(InputEvent::Back); EXPECT_EQ(0, w.stepForTest());   // Welcome: Back no-op
}

TEST(OnboardingApplet, RegionSelectPicksAndAutoAdvances) {
  FakeApp app; AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  gotoStep(w, 2);                       // Radio region
  ASSERT_EQ(2, w.stepForTest());
  w.onInput(InputEvent::NavDown);       // focus preset (region+1)
  int picked = w.focusForTest();
  w.onInput(InputEvent::Select);        // pick + auto-advance
  EXPECT_EQ(picked, w.regionForTest());
  EXPECT_EQ(3, w.stepForTest());        // advanced off Region
}

TEST(OnboardingApplet, RegionPreselectsCurrentRadio) {
  int usa = -1;
  for (int i = 0; i < PRESET_COUNT; i++)
    if (strcmp(PRESETS[i].name, "USA/Canada") == 0) usa = i;
  ASSERT_GE(usa, 0); ASSERT_NE(0, usa);
  FakeApp app;
  app.radio.freqMhz = PRESETS[usa].freq; app.radio.bwKhz = PRESETS[usa].bw;
  app.radio.sf = PRESETS[usa].sf; app.radio.cr = PRESETS[usa].cr;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(usa, w.regionForTest());
}

TEST(OnboardingApplet, FinishAppliesChangedAndSkipsUnchanged) {
  g_doneCalls = 0;
  FakeApp app; app.gpsSup = true; app.tzCity = 5; app.autoSync = true; app.gpsOn = false;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.configure(onDone, nullptr); w.onStart(ctx);
  for (int i = 0; i < 12 && w.stepForTest() < w.stepCountForTest() - 1; i++) advance(w);
  ASSERT_EQ(w.stepCountForTest() - 1, w.stepForTest());   // Done
  advance(w);   // Select -> finish
  EXPECT_TRUE(app.didSetRadio);                 // band changed from the no-match default
  EXPECT_FLOAT_EQ(915.800f, app.setRadioCfg.freqMhz);
  EXPECT_EQ(20, app.setRadioCfg.txPowerDbm);
  EXPECT_TRUE(app.didComplete);
  EXPECT_EQ(1, g_doneCalls);
  EXPECT_STREQ("", app.setName);                // unchanged -> skipped
  EXPECT_FALSE(app.didSetGps);
  EXPECT_FALSE(app.didSetTz);
  EXPECT_FALSE(app.didSetAuto);
}

TEST(OnboardingApplet, ChangedGpsToggleIsApplied) {
  FakeApp app; app.gpsSup = true; app.gpsOn = false;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.configure(onDone, nullptr); w.onStart(ctx);
  gotoStep(w, 3);                       // GPS step
  ASSERT_EQ(3, w.stepForTest());
  w.onInput(InputEvent::Select);        // focus 0 = GPS toggle -> On
  for (int i = 0; i < 12 && w.stepForTest() < w.stepCountForTest() - 1; i++) advance(w);
  advance(w);   // finish
  EXPECT_TRUE(app.didSetGps);
  EXPECT_TRUE(app.setGpsVal);
}

TEST(OnboardingApplet, RendersEveryStepWithoutCrashing) {
  FakeApp app; app.gpsSup = true;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  for (int i = 0; i < w.stepCountForTest(); i++) {
    w.onRender(c);
    EXPECT_GT(d.fills.size(), 0u);
    if (i < w.stepCountForTest() - 1) advance(w);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
