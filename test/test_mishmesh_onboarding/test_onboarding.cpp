#include <gtest/gtest.h>
#include <mishmesh/applets/OnboardingApplet.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
#include <string.h>

using namespace mishmesh;

namespace {
// Records every apply the wizard performs at Finish.
struct FakeApp : AppServices {
  char name[32] = "OLD";
  RadioConfig radio{910.0f, 250.0f, 10, 5, 20, false};
  bool gpsSup = true, gpsOn = false, autoSync = true;
  int tzCity = -1;
  // captured
  char setName[32] = {0}; bool didSetRadio = false; RadioConfig setRadioCfg{};
  bool didSetGps = false, setGpsVal = false;
  int setTzVal = -99; bool didSetTz = false;
  bool didSetAuto = false, setAutoVal = true;
  bool didComplete = false;

  const char* nodeName() const override { return name; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 1782993600u; }
  void selfPublicKeyHex(char* out, size_t cap, int bytes) const override {
    (void)bytes; snprintf(out, cap, "A1B2C3D4");
  }
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

// Advance to the last step (Done) by pressing NavRight stepCount-1 times.
void gotoDone(OnboardingApplet& w) {
  int guard = 0;
  while (w.stepForTest() < w.stepCountForTest() - 1 && guard++ < 10)
    w.onInput(InputEvent::NavRight);
}
}  // namespace

TEST(ShouldShowOnboarding, TruthTable) {
  EXPECT_FALSE(shouldShowOnboarding(2, true));   // DONE never shows
  EXPECT_FALSE(shouldShowOnboarding(2, false));
  EXPECT_TRUE (shouldShowOnboarding(1, false));  // IN_PROGRESS always resumes
  EXPECT_TRUE (shouldShowOnboarding(1, true));
  EXPECT_TRUE (shouldShowOnboarding(0, true));   // NOT_STARTED + fresh -> show
  EXPECT_FALSE(shouldShowOnboarding(0, false));  // NOT_STARTED + existing -> home
}

TEST(OnboardingApplet, GpsStepPresentWhenSupported) {
  FakeApp app; app.gpsSup = true;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(6, w.stepCountForTest());   // Welcome,Name,Region,Gps,Time,Done
}

TEST(OnboardingApplet, GpsStepHiddenWhenUnsupported) {
  FakeApp app; app.gpsSup = false;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(5, w.stepCountForTest());   // Welcome,Name,Region,Time,Done
}

TEST(OnboardingApplet, BackAndNextMoveSteps) {
  FakeApp app; AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(0, w.stepForTest());
  w.onInput(InputEvent::NavRight); EXPECT_EQ(1, w.stepForTest());
  w.onInput(InputEvent::NavLeft);  EXPECT_EQ(0, w.stepForTest());
  w.onInput(InputEvent::NavLeft);  EXPECT_EQ(0, w.stepForTest());  // Welcome: Back ignored
}

TEST(OnboardingApplet, RegionNavPicksPreset) {
  FakeApp app; AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  w.onInput(InputEvent::NavRight);   // -> Name
  w.onInput(InputEvent::NavRight);   // -> Region
  int first = w.regionForTest();
  w.onInput(InputEvent::NavDown);    // next preset
  EXPECT_NE(first, w.regionForTest());
}

TEST(OnboardingApplet, FinishAppliesEverythingAndHandsOff) {
  g_doneCalls = 0;
  FakeApp app; app.gpsSup = true; app.tzCity = 5; app.autoSync = true;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.configure(onDone, nullptr); w.onStart(ctx);
  gotoDone(w);
  EXPECT_EQ(w.stepCountForTest() - 1, w.stepForTest());  // on Done
  w.onInput(InputEvent::Select);     // Finish
  EXPECT_STREQ("OLD", app.setName);  // name defaulted from nodeName(), applied
  EXPECT_TRUE(app.didSetRadio);
  EXPECT_FLOAT_EQ(915.800f, app.setRadioCfg.freqMhz);  // PRESETS[0] = Australia
  EXPECT_EQ(20, app.setRadioCfg.txPowerDbm);           // preserved from current radioConfig()
  EXPECT_TRUE(app.didSetGps);
  EXPECT_TRUE(app.didSetTz);   EXPECT_EQ(5, app.setTzVal);
  EXPECT_TRUE(app.didSetAuto);
  EXPECT_TRUE(app.didComplete);
  EXPECT_EQ(1, g_doneCalls);
}

TEST(OnboardingApplet, RegionPreselectsCurrentRadio) {
  // onStart should highlight the preset matching the device's current radio,
  // so an untouched Region step keeps the existing band (not PRESETS[0]).
  int usa = -1;
  for (int i = 0; i < PRESET_COUNT; i++)
    if (strcmp(PRESETS[i].name, "USA/Canada") == 0) usa = i;
  ASSERT_GE(usa, 0);
  ASSERT_NE(0, usa);   // must differ from the reset-selection default to be meaningful
  FakeApp app;
  app.radio.freqMhz = PRESETS[usa].freq; app.radio.bwKhz = PRESETS[usa].bw;
  app.radio.sf = PRESETS[usa].sf; app.radio.cr = PRESETS[usa].cr;
  AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  EXPECT_EQ(usa, w.regionForTest());
}

TEST(OnboardingApplet, RendersWithoutCrashing) {
  FakeApp app; AppletContext ctx; ctx.app = &app;
  OnboardingApplet w; w.onStart(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  w.onRender(c);
  EXPECT_GT(d.fills.size(), 0u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
