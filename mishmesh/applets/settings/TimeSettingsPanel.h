#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

namespace sound { class SoundEngine; }

// Time settings: timezone offset (stepper), 12/24h format (toggle-in-place),
// alarm/timer ring tunes (push the shared sound picker), "Set automatically"
// (toggle), and manual "Set date & time" (opens SetTimeApplet; hidden while
// auto is on). Source of truth: AppServices (+ ClockService for the tunes).
class TimeSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Time & date"; }
  void begin(AppletContext& ctx) override;
  void onShow() override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editingTz || _editingDateFmt; }

  int  rowCountForTest() const { return _model.count(); }
  bool autoToggleForTest() const { return _model.toggleState(Model::SetAuto); }

private:
  struct Model : ListModel {
    AppServices* app = nullptr;
    enum Row : int { TimeZone, TimeFmt, DateFmt, AlarmSound, AlarmVolume,
                     TimerSound, TimerVolume, SetAuto, SetTime };
    int count() const override { return (app && app->autoTimeSync()) ? 8 : 9; }
    const char* label(int i) const override;
    bool isToggle(int i) const override { return i == SetAuto; }
    bool toggleState(int i) const override { return app && app->autoTimeSync(); }
    const char* value(int i) const override;
  } _model;

  AppServices*  _app = nullptr;
  AppletHost*   _host = nullptr;
  sound::SoundEngine* _snd = nullptr;   // ring preview on volume cycle
  ListMenu      _list;
  StepperDialog _stepper;   // shared modal: timezone or date-format (one at a time)
  bool          _editingTz = false;
  bool          _editingDateFmt = false;
};

TimeSettingsPanel& timeSettings();   // shared singleton

}  // namespace mishmesh
