#include <gtest/gtest.h>
#include <mishmesh/applets/settings/RadioPresets.h>
using namespace mishmesh;

TEST(RadioPresets, TableHas17Entries) {
  EXPECT_EQ(PRESET_COUNT, 17);
}

TEST(RadioPresets, UsaCanadaHasNoRecommendedSuffix) {
  bool found = false;
  for (int i = 0; i < PRESET_COUNT; i++)
    if (strcmp(PRESETS[i].name, "USA/Canada") == 0) found = true;
  EXPECT_TRUE(found);
}

TEST(RadioPresets, MatchExactTupleReturnsIndex) {
  // USA/Canada: 910.525 / SF7 / BW62.5 / CR5
  int i = matchPreset(910.525f, 62.5f, 7, 5);
  ASSERT_GE(i, 0);
  EXPECT_STREQ(PRESETS[i].name, "USA/Canada");
}

TEST(RadioPresets, MatchToleratesFloatEpsilon) {
  EXPECT_GE(matchPreset(910.5251f, 62.50001f, 7, 5), 0);
}

TEST(RadioPresets, NoMatchReturnsMinusOne) {
  EXPECT_EQ(matchPreset(902.0f, 500.0f, 12, 8), -1);
}

#include <mishmesh/core/Applet.h>

namespace {
// Minimal AppServices with radio backing store, for panel/seam tests.
class FakeRadioApp : public mishmesh::AppServices {
public:
  mishmesh::RadioConfig cfg{910.525f, 62.5f, 7, 5, 20};
  int8_t maxTx = 22;
  int setCalls = 0;
  const char* nodeName() const override { return "node"; }
  uint16_t batteryMillivolts() const override { return 4100; }
  uint32_t epochSeconds() const override { return 0; }
  bool radioConfig(mishmesh::RadioConfig& out) const override { out = cfg; return true; }
  int8_t txPowerMax() const override { return maxTx; }
  void setRadioConfig(const mishmesh::RadioConfig& c) override { cfg = c; setCalls++; }
  // repeater seam
  float savedFreq = 0.0f;
  bool repeaterMode() const override { return cfg.repeater; }
  float savedRepeatFreq() const override { return savedFreq; }
  void setSavedRepeatFreq(float f) override { savedFreq = f; }
};
}

TEST(RadioSeam, DefaultBaseReturnsUnavailable) {
  struct BareApp : mishmesh::AppServices {
    const char* nodeName() const override { return ""; }
    uint16_t batteryMillivolts() const override { return 0; }
    uint32_t epochSeconds() const override { return 0; }
  } app;
  mishmesh::RadioConfig c;
  EXPECT_FALSE(app.radioConfig(c));
  EXPECT_EQ(app.txPowerMax(), 22);   // framework default ceiling
}

TEST(RadioSeam, FakeRoundTrips) {
  FakeRadioApp app;
  mishmesh::RadioConfig c;
  ASSERT_TRUE(app.radioConfig(c));
  EXPECT_FLOAT_EQ(c.freqMhz, 910.525f);
  c.sf = 9;
  app.setRadioConfig(c);
  EXPECT_EQ(app.cfg.sf, 9);
  EXPECT_EQ(app.setCalls, 1);
}

#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/applets/RadioValuePickerApplet.h>

namespace {
// Records staged writes for picker tests.
class FakeTarget : public mishmesh::RadioStagingTarget {
public:
  mishmesh::RadioConfig cfg{910.525f, 62.5f, 7, 5, 20};
  const mishmesh::RadioConfig& stagedConfig() const override { return cfg; }
  void stageFrequency(float m) override { cfg.freqMhz = m; }
  void stageBandwidth(float k) override { cfg.bwKhz = k; }
  void stageSf(uint8_t v) override { cfg.sf = v; }
  void stageCr(uint8_t v) override { cfg.cr = v; }
  void stageTxPower(int d) override { cfg.txPowerDbm = (int8_t)d; }
  void stagePreset(int) override {}
  void stageRepeater(bool) override {}
};
}

TEST(RadioValuePicker, SfListSelectionStagesValue) {
  FakeTarget t;
  mishmesh::RadioValuePickerApplet& p = mishmesh::radioValuePickerApplet();
  p.configure(&t, mishmesh::RadioField::SF, "Spreading factor");
  mishmesh::AppletContext ctx;
  p.onStart(ctx);
  EXPECT_EQ(p.count(), 8);                       // SF 5..12
  // radio mark opens on the current value (SF7 -> index 2)
  EXPECT_TRUE(p.radioOn(2));
  p.selectRowForTest(0);                         // SF5
  p.onInput(mishmesh::InputEvent::Select);
  EXPECT_EQ(t.cfg.sf, 5);
}

TEST(RadioValuePicker, CrListHasFourOptions) {
  FakeTarget t;
  mishmesh::RadioValuePickerApplet& p = mishmesh::radioValuePickerApplet();
  p.configure(&t, mishmesh::RadioField::CR, "Coding rate");
  mishmesh::AppletContext ctx; p.onStart(ctx);
  EXPECT_EQ(p.count(), 4);                        // CR 5..8
}

TEST(RadioValuePicker, BandwidthListStagesFloat) {
  FakeTarget t;
  mishmesh::RadioValuePickerApplet& p = mishmesh::radioValuePickerApplet();
  p.configure(&t, mishmesh::RadioField::Bandwidth, "Bandwidth");
  mishmesh::AppletContext ctx; p.onStart(ctx);
  EXPECT_EQ(p.count(), 10);                       // 7.8..500
  p.selectRowForTest(p.count() - 1);              // 500 kHz
  p.onInput(mishmesh::InputEvent::Select);
  EXPECT_FLOAT_EQ(t.cfg.bwKhz, 500.0f);
}

#include <mishmesh/applets/RadioPresetPickerApplet.h>

TEST(RadioPresetPicker, ListsAllPresetsAndStagesSelection) {
  FakeTarget t;   // reuse from Task 4
  mishmesh::RadioPresetPickerApplet& p = mishmesh::radioPresetPickerApplet();
  p.configure(&t);
  mishmesh::AppletContext ctx; p.onStart(ctx);
  EXPECT_EQ(p.count(), mishmesh::PRESET_COUNT);
  EXPECT_STREQ(p.label(0), "Australia");
  // stagePreset forwards the row index to the target.
  int called = -1;
  struct RecTarget : FakeTarget { int* out; void stagePreset(int i) override { *out = i; } };
  RecTarget rt; rt.out = &called; p.configure(&rt);
  p.onStart(ctx);
  p.selectRowForTest(0);
  p.onInput(mishmesh::InputEvent::Select);
  EXPECT_EQ(called, 0);
}

TEST(RadioPresetPicker, RadioMarkFollowsMatchingConfig) {
  FakeTarget t;
  t.cfg = {910.525f, 62.5f, 7, 5, 20};   // == USA/Canada
  mishmesh::RadioPresetPickerApplet& p = mishmesh::radioPresetPickerApplet();
  p.configure(&t);
  mishmesh::AppletContext ctx; p.onStart(ctx);
  int usa = mishmesh::matchPreset(910.525f, 62.5f, 7, 5);
  ASSERT_GE(usa, 0);
  EXPECT_TRUE(p.radioOn(usa));
}

#include <mishmesh/applets/settings/RadioSettingsPanel.h>

static mishmesh::AppletContext ctxWith(mishmesh::AppServices* app) {
  mishmesh::AppletContext c; c.app = app; return c;
}

TEST(RadioPanel, RowValuesReflectConfig) {
  FakeRadioApp app;
  app.cfg = {910.525f, 62.5f, 7, 5, 22};
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  auto ctx = ctxWith(&app);
  panel.begin(ctx);
  ASSERT_EQ(panel.rowCountForTest(), 7);
  EXPECT_STREQ(panel.rowValueForTest(0), "USA/Canada");   // preset match
  EXPECT_STREQ(panel.rowValueForTest(1), "910.525 MHz");
  EXPECT_STREQ(panel.rowValueForTest(2), "62.5 kHz");
  EXPECT_STREQ(panel.rowValueForTest(3), "SF7");
  EXPECT_STREQ(panel.rowValueForTest(4), "CR5");
  EXPECT_STREQ(panel.rowValueForTest(5), "22 dBm");
}

TEST(RadioPanel, NonPresetShowsCustom) {
  FakeRadioApp app;
  app.cfg = {902.0f, 500.0f, 12, 8, 10};
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  auto ctx = ctxWith(&app);
  panel.begin(ctx);
  EXPECT_STREQ(panel.rowValueForTest(0), "Custom");
}

TEST(RadioPanel, StageFrequencyParsesAndClamps) {
  FakeRadioApp app;
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  auto ctx = ctxWith(&app);
  panel.begin(ctx);
  panel.stageFrequency(869.618f);
  EXPECT_STREQ(panel.rowValueForTest(1), "869.618 MHz");
  panel.stageFrequency(99.0f);      // below floor -> clamp to 150
  EXPECT_FLOAT_EQ(panel.stagedConfig().freqMhz, 150.0f);
  panel.stageFrequency(9999.0f);    // above ceiling -> clamp to 2500
  EXPECT_FLOAT_EQ(panel.stagedConfig().freqMhz, 2500.0f);
}

TEST(RadioPanel, StageTxPowerClampsToBoardMax) {
  FakeRadioApp app; app.maxTx = 22;
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  auto ctx = ctxWith(&app);
  panel.begin(ctx);
  panel.stageTxPower(100);          // above board max -> 22
  EXPECT_EQ(panel.stagedConfig().txPowerDbm, 22);
  panel.stageTxPower(-50);          // below floor -> -9
  EXPECT_EQ(panel.stagedConfig().txPowerDbm, -9);
}

TEST(RadioPanel, StagePresetSetsFourFieldsNotTxPower) {
  FakeRadioApp app; app.cfg = {910.525f, 62.5f, 7, 5, 17};
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  auto ctx = ctxWith(&app);
  panel.begin(ctx);
  int nz = mishmesh::matchPreset(917.375f, 250.0f, 11, 5);   // New Zealand
  ASSERT_GE(nz, 0);
  panel.stagePreset(nz);
  EXPECT_FLOAT_EQ(panel.stagedConfig().freqMhz, 917.375f);
  EXPECT_EQ(panel.stagedConfig().sf, 11);
  EXPECT_EQ(panel.stagedConfig().txPowerDbm, 17);   // TX power untouched
}

TEST(RadioPanel, OnHideAppliesOnlyWhenDirty) {
  FakeRadioApp app;
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  auto ctx = ctxWith(&app);
  panel.begin(ctx);
  panel.onHide();
  EXPECT_EQ(app.setCalls, 0);       // no edits -> no apply
  panel.stageSf(9);
  panel.onHide();
  EXPECT_EQ(app.setCalls, 1);       // dirty -> applied once
  EXPECT_EQ(app.cfg.sf, 9);
}

TEST(RadioSettingsPanel, RepeaterRowShowsOffWhenDisabled) {
  FakeRadioApp app;
  app.cfg = {915.8f, 250.0f, 10, 5, 20, false};
  mishmesh::AppletContext ctx = ctxWith(&app);
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  panel.begin(ctx);
  EXPECT_STREQ("Off", panel.rowValueForTest(6));   // row 6 = Repeater
}

TEST(RadioSettingsPanel, RepeaterRowShowsFrequencyWhenEnabled) {
  FakeRadioApp app;
  app.cfg = {869.495f, 250.0f, 10, 5, 20, true};
  mishmesh::AppletContext ctx = ctxWith(&app);
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  panel.begin(ctx);
  EXPECT_STREQ("869.495 MHz", panel.rowValueForTest(6));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
