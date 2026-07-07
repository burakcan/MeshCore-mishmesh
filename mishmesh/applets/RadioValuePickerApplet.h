#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class Canvas;

// Generic single-value radio-select list for Bandwidth / SF / CR. Configure with
// the target + field before push(); Select stages the chosen value and Back pops.
// Launch-only singleton (like SoundPickerApplet).
class RadioValuePickerApplet : public Applet, public ListModel {
public:
  RadioValuePickerApplet();
  void configure(RadioStagingTarget* tgt, RadioField field, const char* title);

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel
  int count() const override;
  const char* label(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

  void selectRowForTest(int i) { _list.setSelected(i); }

private:
  const float* options() const;      // active option array for _field
  int currentIndex() const;          // index of staged value, or 0

  RadioStagingTarget* _tgt = nullptr;
  AppServices* _app = nullptr;
  RadioField _field = RadioField::SF;
  char _title[24] = {0};
  mutable char _lbl[16];
  ListMenu  _list;
  StatusBar _bar;
};

RadioValuePickerApplet& radioValuePickerApplet();

}  // namespace mishmesh
