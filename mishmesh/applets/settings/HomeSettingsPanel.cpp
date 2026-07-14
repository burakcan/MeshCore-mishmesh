#include <mishmesh/applets/settings/HomeSettingsPanel.h>
#include <mishmesh/applets/SettingsDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ScreenSleep.h>
#include <mishmesh/widgets/StepperDialog.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

// --- QuickActionPickerPanel ---

void QuickActionPickerPanel::begin(AppletContext&) {
  _count = 0;   // same order-sorted collection as the app menu
  for (const AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->placement != Placement::AppMenu || _count >= MAX_ENTRIES) continue;
    int i = _count++;
    while (i > 0 && _entries[i - 1]->order > r->order) {
      _entries[i] = _entries[i - 1];
      i--;
    }
    _entries[i] = r;
  }
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
}

bool QuickActionPickerPanel::radioOn(int i) const {
  return strcmp(_entries[i]->label, uiPrefs().quickActionLabel(_slot)) == 0;
}

int QuickActionPickerPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool QuickActionPickerPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _count > 0) {
    uiPrefs().setQuickAction(_slot, _entries[_list.selected()]->label);
    return true;
  }
  return false;   // Back bubbles -> host pops to the Home panel
}

QuickActionPickerPanel& quickActionPicker() {
  static QuickActionPickerPanel s;
  return s;
}

// --- HomeSettingsPanel ---

static void sleepStepLabel(int idx, char* out, uint16_t cap) {
  snprintf(out, cap, "%s", screenSleepLabel(idx));
}

static void brightnessStepLabel(int idx, char* out, uint16_t cap) {
  static const char* const LABELS[] = { "Minimum", "Low", "Medium", "High", "Maximum" };
  snprintf(out, cap, "%s", LABELS[idx < 5 ? idx : 4]);
}

const char* HomeSettingsPanel::Model::label(int i) const {
  static const char* const LABELS[ROW_COUNT] = {
    "Battery percent", "Screen sleep", "Left shortcut", "Right shortcut", "Screen brightness" };
  return (i >= 0 && i < ROW_COUNT) ? LABELS[i] : "";
}

bool HomeSettingsPanel::Model::toggleState(int i) const {
  return i == BattPercent && uiPrefs().battShowPercent();
}

const char* HomeSettingsPanel::Model::value(int i) const {
  if (i == ScreenSleep) return app ? screenSleepLabel(app->screenSleepIndex()) : "";
  if (i == LeftAction)  return uiPrefs().quickActionLabel(UiPrefs::SLOT_LEFT);
  if (i == RightAction) return uiPrefs().quickActionLabel(UiPrefs::SLOT_RIGHT);
  if (i == ScreenBrightness && app) {
    static char label[12];
    brightnessStepLabel(app->screenBrightnessIndex(), label, sizeof(label));
    return label;
  }
  return nullptr;
}

void HomeSettingsPanel::begin(AppletContext& ctx) {
  _host = ctx.host;
  _model.app = ctx.app;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();   // singleton reuse: setModel skips reset on same-ptr rebind
  _editingSleep = false;
  _editingBrightness = false;
}

int HomeSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  if (_editingSleep || _editingBrightness) {
    _stepper.draw(c, 0, 0, c.width(), c.height());
    return 100;
  }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool HomeSettingsPanel::onInput(InputEvent ev) {
  if (_editingSleep || _editingBrightness) {
    if (_stepper.onInput(ev)) {
      StepperResult r = _stepper.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _model.app) {
          if (_editingSleep) _model.app->setScreenSleepIndex((uint8_t)_stepper.value());
          else _model.app->setScreenBrightnessIndex((uint8_t)_stepper.value());
        }
        _editingSleep = _editingBrightness = false;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int i = _list.selected();
    if (i == Model::BattPercent) {
      uiPrefs().setBattShowPercent(!uiPrefs().battShowPercent());
    } else if (i == Model::ScreenSleep) {
      if (_model.app) {
        _stepper.configure("Screen sleep", _model.app->screenSleepIndex(),
                           0, SCREEN_SLEEP_COUNT - 1, sleepStepLabel);
        _editingSleep = true;
      }
    } else if (i == Model::ScreenBrightness) {
      if (_model.app) {
        _stepper.configure("Screen brightness", _model.app->screenBrightnessIndex(),
                           0, 4, brightnessStepLabel);
        _editingBrightness = true;
      }
    } else if (_host) {
      static SettingsDetailApplet detail;   // one level below the shared detail
      quickActionPicker().setSlot(i == Model::LeftAction ? UiPrefs::SLOT_LEFT
                                                         : UiPrefs::SLOT_RIGHT);
      detail.setPanel(&quickActionPicker());
      _host->push(&detail);
    }
    return true;
  }
  return false;   // Back bubbles
}

HomeSettingsPanel& homeSettings() {
  static HomeSettingsPanel s;
  return s;
}

}  // namespace mishmesh
