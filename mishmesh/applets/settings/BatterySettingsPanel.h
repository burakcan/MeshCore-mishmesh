#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

// All battery settings: indicator style (Gauge/Percent/Voltage) and the ADC
// calibration trim (with a live voltage readout). Both open a modal stepper on
// Select, matching the Home panel's sleep/brightness rows. Shared singleton.
class BatterySettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Battery"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editing != Editing::None; }

private:
  struct Model : ListModel {
    AppServices* app = nullptr;
    enum Row : int { Display, Calibration, ROW_COUNT };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override;
    const char* value(int i) const override;
    mutable char _buf[8];
  } _model;

  // Which row's stepper is open (only one at a time).
  enum class Editing : uint8_t { None, Display, Cal };

  AppServices* _app = nullptr;
  ListMenu      _list;
  StepperDialog _stepper;
  Editing _editing = Editing::None;
  int  _calRestore = 100;
};

BatterySettingsPanel& batterySettings();

}  // namespace mishmesh
