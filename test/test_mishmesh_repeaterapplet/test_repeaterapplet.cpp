#include <gtest/gtest.h>
#include <mishmesh/applets/RepeaterApplet.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/core/Applet.h>
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
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
