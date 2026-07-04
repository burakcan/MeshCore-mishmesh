// mishmesh/applets/RepeaterRadioPanel.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/widgets/FormView.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Edits a remote repeater's radio params over the CLI channel. Implements
// RadioStagingTarget so it reuses RadioPreset/ValuePickerApplet. Fetches via
// `get radio`/`get tx`; saves via `set radio f bw sf cr` (reboot to apply) then
// `set tx dbm`. Launch-only (pushed from the Settings hub's Radio row). Host-safe.
class RepeaterRadioPanel : public Applet, public RadioStagingTarget {
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
  int   focusForTest() const { return _view.focus(); }

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
  FormView _view;
  ConfirmDialog _confirm;

  void fireGet(const char* cmd, uint32_t nowMs);
  void fireSet(const char* cmd, uint32_t nowMs);
  bool anyDirty() const { return _radioDirty || _txDirty; }
  void beginSave();
  static void onEditDone(void* ctx, const char* text);
};

RepeaterRadioPanel& repeaterRadioPanel();

}  // namespace mishmesh
