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
  int  bodyHeightForTest() const { return _lastBodyH; }     // area the list+header share
  int  captionHeightForTest() const { return _lastCaptionH; }
  int  rowHeightForTest() const { return _list.rowHeight(); }

private:
  // The description, drawn as a ListMenu header so it scrolls together with the
  // frequency rows: the whole screen moves as one unit. Kept short (see INFO) so
  // that at the top of the scroll the options are still visible below it.
  class InfoHeader : public Widget {
  public:
    const char* text = nullptr;
    void draw(Canvas& c, int x, int y, int w, int h) override;
  };

  RadioStagingTarget* _tgt = nullptr;
  AppServices* _app = nullptr;
  mutable char _lbl[16];
  ListMenu   _list;
  StatusBar  _bar;
  InfoHeader _info;
  int _lastBodyH = 0;      // body height handed to the list on the last render (test seam)
  int _lastCaptionH = 0;   // measured header height on the last render (test seam)
};

RepeaterApplet& repeaterApplet();

}  // namespace mishmesh
