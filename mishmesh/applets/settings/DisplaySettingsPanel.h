#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

class AppletHost;

// Pick-one list for a small fixed set of choices, hosted one level below the
// Display panel. Both the orientation and the input-rotation rows use it, so the
// labels and the commit are handed in rather than baked in.
class ChoicePickerPanel : public SettingsPanel, public ListModel {
public:
  typedef void (*Commit)(int choice);
  void configure(const char* title, const char* const* labels, int count,
                 int current, Commit commit);

  const char* title() const override { return _title; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

  int count() const override { return _count; }
  const char* label(int i) const override { return _labels[i]; }
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override { return i == _current; }

private:
  const char* _title = "";
  const char* const* _labels = nullptr;
  int _count = 0;
  int _current = 0;
  Commit _commit = nullptr;
  ListMenu _list;
};

ChoicePickerPanel& choicePicker();

// Panel-wide settings: how large the interface is drawn, when the screen sleeps,
// and how bright it is. These used to sit under Home, which was the wrong place -
// none of them are about the home face.
class DisplaySettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Display"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editingSleep || _editingBrightness; }

  const char* rowLabelForTest(int i) const { return _model.label(i); }
  const char* rowValueForTest(int i) const { return _model.value(i); }

private:
  struct Model : ListModel {
    AppServices* app = nullptr;
    bool sizeSupported = false;      // only panels that can magnify offer the row
    bool rotateSupported = false;    // ditto for panels that can be turned
    enum Row : int { InterfaceSize, Orientation, InputRotation, ScreenSleep,
                     ScreenBrightness, ROW_COUNT };
    int rowAt(int visible) const;    // visible index -> Row, skipping hidden ones
    int count() const override;
    const char* label(int i) const override;
    const char* value(int i) const override;
  } _model;

  static void commitRotation(int choice);
  static void commitInputRotation(int choice);
  void pushPicker(const char* title, const char* const* labels, int count,
                  int current, void (*commit)(int));

  AppletHost* _host = nullptr;
  ListMenu _list;
  StepperDialog _stepper;
  bool _editingSleep = false;
  bool _editingBrightness = false;
  uint8_t _brightnessRestore = 4;   // index to revert to if the stepper is cancelled
};

DisplaySettingsPanel& displaySettings();

}  // namespace mishmesh
