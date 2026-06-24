#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/settings/RadioStaging.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class Canvas;

// Manual off-grid repeater picker: a short description plus a pick-one list of
// Off + the firmware-permitted repeat frequencies. Stages the choice into the
// Radio panel (applied on its onHide). Launch-only singleton.
class RepeaterApplet : public Applet, public ListModel {
public:
  RepeaterApplet();
  void configure(RadioStagingTarget* tgt) { _tgt = tgt; }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel: row 0 = Off, rows 1..N = allowed frequencies.
  int count() const override;
  const char* label(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

  void selectRowForTest(int i) { _list.setSelected(i); }

private:
  RadioStagingTarget* _tgt = nullptr;
  AppServices* _app = nullptr;
  mutable char _lbl[16];
  ListMenu  _list;
  StatusBar _bar;
};

RepeaterApplet& repeaterApplet();

}  // namespace mishmesh
