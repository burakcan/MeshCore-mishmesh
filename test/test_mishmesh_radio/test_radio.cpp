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

#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/RepeaterRadioPanel.h>

TEST(RepeaterRadio, FetchesRadioThenTx) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterRadioPanel().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterRadioPanel());
  host.loop(1);                             // fires "get radio"
  EXPECT_EQ("get radio", svc.lastCli);
  svc.simulateCliReply(pk, "> 910.5,250,10,5");
  host.loop(200);                           // parses radio, fires "get tx"
  EXPECT_EQ("get tx", svc.lastCli);
  svc.simulateCliReply(pk, "> 22");
  host.loop(400);                           // parses tx
  EXPECT_FLOAT_EQ(910.5f, mishmesh::repeaterRadioPanel().freqForTest());
  EXPECT_EQ(22, mishmesh::repeaterRadioPanel().txForTest());
}

TEST(RepeaterRadio, StagingSetsDirtyFlags) {
  mishmesh::RepeaterRadioPanel& p = mishmesh::repeaterRadioPanel();
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  p.setTarget(pk, "Rep1"); host.setRoot(&p); host.loop(1);
  svc.simulateCliReply(pk, "> 910.5,250,10,5"); host.loop(200);
  svc.simulateCliReply(pk, "> 22"); host.loop(400);

  p.stageFrequency(868.0f);
  EXPECT_TRUE(p.radioDirtyForTest());
  EXPECT_FALSE(p.txDirtyForTest());
  p.stageTxPower(14);
  EXPECT_TRUE(p.txDirtyForTest());
  EXPECT_FLOAT_EQ(868.0f, p.freqForTest());
  EXPECT_EQ(14, p.txForTest());
}

TEST(RepeaterRadio, SaveSendsRadioThenTx) {
  mishmesh::RepeaterRadioPanel& p = mishmesh::repeaterRadioPanel();
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  p.setTarget(pk, "Rep1"); host.setRoot(&p); host.loop(1);
  svc.simulateCliReply(pk, "> 910.5,250,10,5"); host.loop(200);
  svc.simulateCliReply(pk, "> 22"); host.loop(400);

  p.stageSf(11);         // radio dirty
  p.stageTxPower(14);    // tx dirty
  // Save is the ListMenu button row at index 6 (6 field rows at 0..5). NavDown 6 times from index 0.
  for (int i = 0; i < 6; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  EXPECT_EQ(6, p.focusForTest());   // confirms focus is on Save button (N)
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(0, strncmp(svc.lastCli.c_str(), "set radio ", 10));   // radio first
  svc.simulateCliReply(pk, "OK - reboot to apply");
  host.loop(600);
  EXPECT_EQ(0, strncmp(svc.lastCli.c_str(), "set tx ", 7));       // then tx
  svc.simulateCliReply(pk, "OK");
  host.loop(800);
  EXPECT_FALSE(p.radioDirtyForTest());
  EXPECT_FALSE(p.txDirtyForTest());
}

TEST(RepeaterRadio, LoadingPhaseBeforeFetchComplete) {
  mishmesh::RepeaterRadioPanel& p = mishmesh::repeaterRadioPanel();
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'L','D','T','0','0','1'};
  p.setTarget(pk, "LoadTest"); host.setRoot(&p); host.loop(1);
  // Panel has fired "get radio" but no reply yet -- still Loading.
  // Attempting Select should not trigger a save command.
  std::string afterStart = svc.lastCli;  // "get radio"
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(afterStart, svc.lastCli);   // no new CLI command sent
}

#include <mishmesh/applets/settings/RadioFormat.h>

namespace {
// GAT562 30S vendor table (869 MHz), chip settings 0..22, tenths of a dBm.
const int16_t GAT_OUT[] = {136, 162, 184, 200, 214, 226, 238, 248, 258, 268, 274, 282,
                           288, 294, 298, 300, 300, 300, 300, 300, 300, 300, 300};
const mishmesh::TxPaTable GAT_PA = {0, 23, GAT_OUT};

class PaRadioApp : public FakeRadioApp {
public:
  const mishmesh::TxPaTable* txPaTable() const override { return &GAT_PA; }
};
}

TEST(TxPa, LookupCoversTableOnly) {
  int16_t out = 0;
  EXPECT_TRUE(mishmesh::txPaOutputDeciDbm(&GAT_PA, 0, out));   EXPECT_EQ(out, 136);
  EXPECT_TRUE(mishmesh::txPaOutputDeciDbm(&GAT_PA, 3, out));   EXPECT_EQ(out, 200);
  EXPECT_TRUE(mishmesh::txPaOutputDeciDbm(&GAT_PA, 14, out));  EXPECT_EQ(out, 298);
  EXPECT_TRUE(mishmesh::txPaOutputDeciDbm(&GAT_PA, 15, out));  EXPECT_EQ(out, 300);
  EXPECT_TRUE(mishmesh::txPaOutputDeciDbm(&GAT_PA, 22, out));  EXPECT_EQ(out, 300);
  EXPECT_FALSE(mishmesh::txPaOutputDeciDbm(&GAT_PA, -1, out));
  EXPECT_FALSE(mishmesh::txPaOutputDeciDbm(&GAT_PA, 23, out));
  EXPECT_FALSE(mishmesh::txPaOutputDeciDbm(nullptr, 10, out));
}

TEST(TxPa, LookupHonoursFirstSetting) {
  const int16_t t[] = {50, 70};
  const mishmesh::TxPaTable pa = {-2, 2, t};
  int16_t out = 0;
  EXPECT_TRUE(mishmesh::txPaOutputDeciDbm(&pa, -2, out));  EXPECT_EQ(out, 50);
  EXPECT_TRUE(mishmesh::txPaOutputDeciDbm(&pa, -1, out));  EXPECT_EQ(out, 70);
  EXPECT_FALSE(mishmesh::txPaOutputDeciDbm(&pa, 0, out));
}

TEST(TxPa, FormatWithoutPaIsPlainDbm) {
  char b[24];
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 22, nullptr), "22 dBm");
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), -9, nullptr), "-9 dBm");
}

TEST(TxPa, FormatShowsMeasuredOutput) {
  char b[24];
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 22, &GAT_PA), "22 -> ~30 dBm");
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 14, &GAT_PA), "14 -> ~29.8 dBm");
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 3, &GAT_PA), "3 -> ~20 dBm");
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 0, &GAT_PA), "0 -> ~13.6 dBm");
  // Below the table there is no measurement; never present the chip value as output.
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), -9, &GAT_PA), "-9 dBm chip");
}

TEST(TxPa, FormatFractionalAndNegativeOutput) {
  const int16_t t[] = {-25, -5, 5};
  const mishmesh::TxPaTable pa = {0, 3, t};
  char b[24];
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 0, &pa), "0 -> ~-2.5 dBm");
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 1, &pa), "1 -> ~-0.5 dBm");
  EXPECT_STREQ(mishmesh::formatTxPower(b, sizeof(b), 2, &pa), "2 -> ~0.5 dBm");
}

TEST(TxPa, PanelRowShowsOutputAndKeepsChipBounds) {
  PaRadioApp app;
  app.cfg = {869.618f, 62.5f, 8, 8, 22};
  mishmesh::RadioSettingsPanel& panel = mishmesh::radioSettings();
  auto ctx = ctxWith(&app);
  panel.begin(ctx);
  EXPECT_STREQ(panel.rowValueForTest(5), "22 -> ~30 dBm");
  panel.stageTxPower(10);
  EXPECT_STREQ(panel.rowValueForTest(5), "10 -> ~27.4 dBm");
  panel.stageTxPower(100);   // the PA table never widens the chip ceiling
  EXPECT_EQ(panel.stagedConfig().txPowerDbm, 22);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
