#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// Radio settings: preset picker + frequency/bandwidth/SF/CR/TX power. Edits a
// staged RadioConfig loaded on begin(); applies once via AppServices on onHide().
class RadioSettingsPanel : public SettingsPanel, public RadioStagingTarget {
public:
  const char* title() const override { return "Radio"; }
  void begin(AppletContext& ctx) override;
  void onHide() override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

  // RadioStagingTarget
  const RadioConfig& stagedConfig() const override { return _staged; }
  void stageFrequency(float mhz) override;
  void stageBandwidth(float khz) override;
  void stageSf(uint8_t sf) override;
  void stageCr(uint8_t cr) override;
  void stageTxPower(int dbm) override;
  void stagePreset(int presetIndex) override;
  void stageRepeater(bool on) override;

  // test seams
  int rowCountForTest() const { return _model.count(); }
  const char* rowValueForTest(int i) const { return _model.value(i); }

private:
  struct Model : ListModel {
    const RadioConfig* cfg = nullptr;
    mutable char vbuf[24];
    enum Row : int { Preset, Freq, Bw, Sf, Cr, Tx, Repeater, ROW_COUNT };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override;
    const char* value(int i) const override;
  } _model;

  // keypad onConfirm trampolines (ctx = this)
  static void onFreqDone(void* ctx, const char* text);
  static void onTxDone(void* ctx, const char* text);
  void editFrequency();
  void editTxPower();

  RadioConfig  _staged{915.0f, 250.0f, 10, 5, 20, false};
  bool         _dirty = false;
  AppServices* _app = nullptr;
  AppletHost*  _host = nullptr;
  char         _scratch[16] = {0};
  ListMenu     _list;
};

RadioSettingsPanel& radioSettings();

}  // namespace mishmesh
