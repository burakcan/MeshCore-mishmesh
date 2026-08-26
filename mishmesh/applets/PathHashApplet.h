#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class Canvas;

// Pick-one list for this node's path-hash size (1/2/3 bytes per hop). Writes the
// choice straight to AppServices::setPathHashMode (the adapter persists it); no
// staging. A scrolling header carries the firmware-compatibility warning.
// Launch-only singleton; mirrors RepeaterApplet.
class PathHashApplet : public Applet, public ListModel {
public:
  PathHashApplet();

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel: rows 0/1/2 = 1/2/3 byte hashes.
  int count() const override { return 3; }
  const char* label(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

  void selectRowForTest(int i) { _list.setSelected(i); }
  int  bodyHeightForTest() const { return _lastBodyH; }
  int  captionHeightForTest() const { return _lastCaptionH; }

private:
  // Warning drawn as a ListMenu header so it scrolls with the rows as one unit.
  class InfoHeader : public Widget {
  public:
    const char* text = nullptr;
    void draw(Canvas& c, int x, int y, int w, int h) override;
  };

  AppServices* _app = nullptr;
  ListMenu   _list;
  StatusBar  _bar;
  InfoHeader _info;
  int _lastBodyH = 0;
  int _lastCaptionH = 0;
};

PathHashApplet& pathHashApplet();

}  // namespace mishmesh
