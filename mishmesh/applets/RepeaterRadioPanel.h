// mishmesh/applets/RepeaterRadioPanel.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/applets/settings/RadioFormat.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Edits a remote repeater's radio params over the CLI channel. Implements
// RadioStagingTarget so it reuses RadioPreset/ValuePickerApplet. Fetches via
// `get radio`/`get tx`; saves via `set radio f bw sf cr` (reboot to apply) then
// `set tx dbm`. Launch-only (pushed from the Settings hub's Radio row). Host-safe.
class RepeaterRadioPanel : public Applet, public RadioStagingTarget, public ListModel {
public:
  RepeaterRadioPanel() : Applet("Radio") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // RadioStagingTarget
  const RadioConfig& stagedConfig() const override { return _cfg; }
  void stageFrequency(float mhz) override;
  void stageBandwidth(float khz) override;
  void stageSf(uint8_t sf) override;
  void stageCr(uint8_t cr) override;
  void stageTxPower(int dbm) override;
  void stagePreset(int presetIndex) override;
  void stageRepeater(bool) override {}   // repeater's off-grid repeat is a separate panel

  float freqForTest() const { return _cfg.freqMhz; }
  int   txForTest() const { return _cfg.txPowerDbm; }
  bool  radioDirtyForTest() const { return _radioDirty; }
  bool  txDirtyForTest() const { return _txDirty; }
  int   focusForTest() const { return _view.selected(); }   // list focus (Save at 6)

  // ListModel (6 radio field rows + Save button at 6)
  int count() const override { return 7; }
  const char* label(int i) const override {
    static const char* L[7] = { "Preset", "Frequency", "Bandwidth", "Spread factor",
                                "Coding rate", "TX power", "Save" };
    return (i >= 0 && i < 7) ? L[i] : "";
  }
  const char* value(int i) const override {
    if (i == 0) { int m = matchPreset(_cfg.freqMhz, _cfg.bwKhz, _cfg.sf, _cfg.cr);
                  return m >= 0 ? PRESETS[m].name : "Custom"; }
    if (i >= 1 && i <= 5) return formatRadioField(_vbuf, sizeof(_vbuf), i, _cfg);
    return nullptr;   // Save row
  }
  bool isButton(int i) const override { return i == 6; }

private:
  enum class Phase : uint8_t { Loading, List, Saving, Editing, Confirm };
  enum class Step  : uint8_t { Radio, Tx, Done };

  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppServices*     _app = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  RadioConfig _cfg{915.0f, 250.0f, 10, 5, 20, false};
  StatusBar _bar;
  bool     _radioDirty = false, _txDirty = false;
  bool     _rebootNote = false, _saveFailed = false;
  Phase    _phase = Phase::Loading;
  Step     _step = Step::Radio;
  uint32_t _startSeq = 0;
  int      _editField = -1;             // 1 = freq, 5 = tx (keypad edits)
  char     _scratch[16] = {0};
  PendingRequest _req;
  ListMenu _view;
  mutable char _vbuf[24];   // scratch for value() (radio-field formatting)
  ConfirmDialog _confirm;

  void fireGet(const char* cmd, uint32_t nowMs);
  void fireSet(const char* cmd, uint32_t nowMs);
  bool anyDirty() const { return _radioDirty || _txDirty; }
  void beginSave();
  static void onEditDone(void* ctx, const char* text);
};

RepeaterRadioPanel& repeaterRadioPanel();

}  // namespace mishmesh
