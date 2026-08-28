#include <mishmesh/applets/settings/DisplaySettingsPanel.h>
#include <mishmesh/applets/SettingsDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ScreenSleep.h>
#include <stdio.h>

namespace mishmesh {

static void sleepStepLabel(int idx, char* out, uint16_t cap) {
  snprintf(out, cap, "%s", screenSleepLabel(idx));
}

static const char* brightnessLabel(int idx) {
  static const char* const LABELS[] = { "Low", "Medium", "High" };
  return LABELS[idx < 3 ? idx : 2];
}

static void brightnessStepLabel(int idx, char* out, uint16_t cap) {
  snprintf(out, cap, "%s", brightnessLabel(idx));
}

// Quarter turns clockwise from the variant's landscape default.
static const char* const ROTATION_LABELS[UiPrefs::ROTATIONS] = {
  "Landscape", "Portrait", "Landscape flipped", "Portrait flipped"
};
// Auto first: following the screen is what rotating it is for.
static const char* const INPUT_LABELS[UiPrefs::ROTATIONS + 1] = {
  "Auto", "Landscape", "Portrait", "Landscape flipped", "Portrait flipped"
};

void ChoicePickerPanel::configure(const char* title, const char* const* labels, int count,
                                  int current, Commit commit) {
  _title = title; _labels = labels; _count = count; _current = current; _commit = commit;
}

void ChoicePickerPanel::begin(AppletContext&) {
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
  for (int i = 0; i < _current; i++) _list.onInput(InputEvent::NavDown);   // open on the current choice
}

int ChoicePickerPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool ChoicePickerPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _count > 0) {
    _current = _list.selected();
    if (_commit) _commit(_current);
    return true;
  }
  return false;   // Back bubbles -> host pops back to Display
}

ChoicePickerPanel& choicePicker() {
  static ChoicePickerPanel s;
  return s;
}

int DisplaySettingsPanel::Model::rowAt(int visible) const {
  int n = 0;
  if (sizeSupported && n++ == visible) return InterfaceSize;
  if (rotateSupported && n++ == visible) return Orientation;
  if (rotateSupported && n++ == visible) return InputRotation;
  if (n++ == visible) return ScreenSleep;
  return ScreenBrightness;
}

int DisplaySettingsPanel::Model::count() const {
  int n = 1;   // screen sleep is always offered
  if (sizeSupported) n++;
  if (rotateSupported) n += 2;   // orientation + the input override that follows it
  if (app && app->screenBrightnessSupported()) n++;
  return n;
}

const char* DisplaySettingsPanel::Model::label(int i) const {
  switch (rowAt(i)) {
    case InterfaceSize:    return "Interface size";
    case Orientation:      return "Orientation";
    case InputRotation:    return "Controls";
    case ScreenSleep:      return "Screen sleep";
    case ScreenBrightness: return "Screen brightness";
  }
  return "";
}

const char* DisplaySettingsPanel::Model::value(int i) const {
  switch (rowAt(i)) {
    case InterfaceSize:    return uiPrefs().uiScale() > 1 ? "Large" : "Standard";
    case Orientation:      return ROTATION_LABELS[uiPrefs().rotation()];
    case InputRotation:    return uiPrefs().inputRotation() == UiPrefs::INPUT_AUTO
                                    ? "Auto" : ROTATION_LABELS[uiPrefs().inputRotation()];
    case ScreenSleep:      return app ? screenSleepLabel(app->screenSleepIndex()) : "";
    case ScreenBrightness: return app ? brightnessLabel(app->screenBrightnessIndex()) : "";
  }
  return "";
}

void DisplaySettingsPanel::begin(AppletContext& ctx) {
  _host = ctx.host;
  _model.app = ctx.app;
  _model.sizeSupported = _host && _host->displaySupportsUiScale();
  _model.rotateSupported = _host && _host->displaySupportsOrientation();
  _editingSleep = _editingBrightness = false;
  _stepper.reset();
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
}

int DisplaySettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  if (_editingSleep || _editingBrightness) {
    _stepper.draw(c, x, y, w, h);
    return 100;
  }
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool DisplaySettingsPanel::onInput(InputEvent ev) {
  if (_editingSleep || _editingBrightness) {
    if (_stepper.onInput(ev)) {
      StepperResult r = _stepper.result();
      if (r == StepperResult::None) {
        // Live preview so the level is judged on the panel, not from its name.
        if (_editingBrightness && _model.app)
          _model.app->previewScreenBrightnessIndex((uint8_t)_stepper.value());
      } else {
        if (_model.app) {
          if (_editingSleep) {
            if (r == StepperResult::Confirmed)
              _model.app->setScreenSleepIndex((uint8_t)_stepper.value());
          } else if (r == StepperResult::Confirmed) {
            _model.app->setScreenBrightnessIndex((uint8_t)_stepper.value());
          } else {
            _model.app->previewScreenBrightnessIndex(_brightnessRestore);   // revert
          }
        }
        _editingSleep = _editingBrightness = false;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    switch (_model.rowAt(_list.selected())) {
      case Model::InterfaceSize: {
        // Two settings, so Select toggles rather than opening a stepper for a
        // choice of two. The relayout is immediate: the list redraws at the new
        // size under the cursor, which is the clearest preview there is.
        const int next = uiPrefs().uiScale() > 1 ? 1 : 2;
        uiPrefs().setUiScale(next);
        if (_host) _host->applyUiScale(next);
        break;
      }
      case Model::Orientation:
        pushPicker("Orientation", ROTATION_LABELS, UiPrefs::ROTATIONS,
                   uiPrefs().rotation(), &commitRotation);
        break;
      case Model::InputRotation:
        pushPicker("Controls", INPUT_LABELS, UiPrefs::ROTATIONS + 1,
                   uiPrefs().inputRotation() == UiPrefs::INPUT_AUTO
                       ? 0 : uiPrefs().inputRotation() + 1,
                   &commitInputRotation);
        break;
      case Model::ScreenSleep:
        if (_model.app) {
          _stepper.configure("Screen sleep", _model.app->screenSleepIndex(),
                             0, SCREEN_SLEEP_COUNT - 1, sleepStepLabel);
          _editingSleep = true;
        }
        break;
      case Model::ScreenBrightness:
        if (_model.app) {
          _brightnessRestore = _model.app->screenBrightnessIndex();
          _stepper.configure("Screen brightness", _brightnessRestore,
                             0, 2, brightnessStepLabel);
          _editingBrightness = true;
        }
        break;
    }
    return true;
  }
  return false;   // Back bubbles
}

// The picker commits through the singleton, which is also how the Display panel
// reaches the host - the callback has no context of its own.
static AppletHost* s_host = nullptr;

void DisplaySettingsPanel::commitRotation(int choice) {
  uiPrefs().setRotation(choice);
  if (s_host) {
    s_host->applyDisplayRotation(choice);
    s_host->setInputRotation(uiPrefs().effectiveInputRotation());
  }
}

void DisplaySettingsPanel::commitInputRotation(int choice) {
  uiPrefs().setInputRotation(choice == 0 ? UiPrefs::INPUT_AUTO : choice - 1);
  if (s_host) s_host->setInputRotation(uiPrefs().effectiveInputRotation());
}

void DisplaySettingsPanel::pushPicker(const char* title, const char* const* labels,
                                      int count, int current, void (*commit)(int)) {
  if (!_host) return;
  s_host = _host;
  static SettingsDetailApplet detail;   // one level below the shared detail
  choicePicker().configure(title, labels, count, current, commit);
  detail.setPanel(&choicePicker());
  _host->push(&detail);
}

DisplaySettingsPanel& displaySettings() {
  static DisplaySettingsPanel s;
  return s;
}

}  // namespace mishmesh
