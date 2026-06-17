#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class Canvas;

// Region-preset radio-select list. Rows show name + a compact summary
// ("915.8MHz / SF10 / BW250 / CR5"). Select stages the preset (freq/sf/bw/cr);
// Back pops. Launch-only singleton.
class RadioPresetPickerApplet : public Applet, public ListModel {
public:
  RadioPresetPickerApplet();
  void configure(RadioStagingTarget* tgt) { _tgt = tgt; }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel
  int count() const override;
  const char* label(int i) const override;
  const char* value(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

  void selectRowForTest(int i) { _list.setSelected(i); }

private:
  RadioStagingTarget* _tgt = nullptr;
  mutable char _summary[28];
  ListMenu  _list;
  StatusBar _bar;
};

RadioPresetPickerApplet& radioPresetPickerApplet();

}  // namespace mishmesh
