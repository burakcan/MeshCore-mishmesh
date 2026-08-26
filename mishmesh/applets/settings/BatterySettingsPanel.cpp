#include <mishmesh/applets/settings/BatterySettingsPanel.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>

namespace mishmesh {

// Live voltage in the stepper label. The preview trim is applied before this
// render, so batteryMillivoltsLive() already reflects the stepped percent.
static AppServices* g_calApp = nullptr;
static void calStepLabel(int pct, char* out, uint16_t cap) {
  uint16_t mv = g_calApp ? g_calApp->batteryMillivoltsLive() : 0;
  snprintf(out, cap, "%d%%  %u.%02uV", pct,
           (unsigned)(mv / 1000), (unsigned)((mv % 1000) / 10));
}

static void modeStepLabel(int idx, char* out, uint16_t cap) {
  const char* n = idx == 1 ? "Percent" : idx == 2 ? "Voltage" : "Gauge";
  snprintf(out, cap, "%s", n);
}

const char* BatterySettingsPanel::Model::label(int i) const {
  return i == Display ? "Display" : i == Calibration ? "Calibration" : "";
}

const char* BatterySettingsPanel::Model::value(int i) const {
  if (i == Display) {
    switch (uiPrefs().battMode()) {
      case UiPrefs::BattMode::Percent: return "Percent";
      case UiPrefs::BattMode::Voltage: return "Voltage";
      default:                         return "Gauge";
    }
  }
  if (i == Calibration && app) {
    snprintf(_buf, sizeof(_buf), "%d%%", app->batteryCalPercent());
    return _buf;
  }
  return nullptr;
}

void BatterySettingsPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _model.app = _app;
  g_calApp = _app;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
  _editing = Editing::None;
}

int BatterySettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  if (_editing != Editing::None) {
    _stepper.draw(c, 0, 0, c.width(), c.height());
    return 100;
  }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool BatterySettingsPanel::onInput(InputEvent ev) {
  if (_editing != Editing::None) {
    if (_stepper.onInput(ev)) {
      StepperResult r = _stepper.result();
      if (r == StepperResult::None) {
        // Calibration previews live; Display can't (the modal scrim hides the bar).
        if (_editing == Editing::Cal && _app) _app->previewBatteryCalibration(_stepper.value());
      } else {
        if (r == StepperResult::Confirmed) {
          if (_editing == Editing::Display)
            uiPrefs().setBattMode((UiPrefs::BattMode)_stepper.value());
          else if (_app)
            _app->setBatteryCalibration(_stepper.value());
        } else if (_editing == Editing::Cal && _app) {
          _app->previewBatteryCalibration(_calRestore);   // cancelled: revert live trim
        }
        _editing = Editing::None;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int i = _list.selected();
    if (i == Model::Display) {
      _stepper.configure("Display", (int)uiPrefs().battMode(), 0, 2, modeStepLabel);
      _editing = Editing::Display;
    } else if (i == Model::Calibration && _app) {
      _calRestore = _app->batteryCalPercent();
      _stepper.configure("Calibration", _calRestore, 50, 150, calStepLabel);
      _editing = Editing::Cal;
    }
    return true;
  }
  return false;   // Back bubbles
}

BatterySettingsPanel& batterySettings() {
  static BatterySettingsPanel s;
  return s;
}

}  // namespace mishmesh
