#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class Canvas;

// Fired with the chosen WorldClock city index when the user selects a row.
typedef void (*TzPickCallback)(void* ctx, int cityIndex);

// Timezone picker: a list of WorldClock cities (name + current effective offset,
// DST included). Preselects the configured index; Select fires the callback with
// the highlighted index and pops; Back pops with no change. Launch-only singleton.
class TimezonePickerApplet : public Applet, public ListModel {
public:
  TimezonePickerApplet();
  void configure(int currentIndex, TzPickCallback cb, void* ctx) {
    _pending = currentIndex; _cb = cb; _ctx = ctx;
  }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel
  int count() const override;
  const char* label(int i) const override;
  const char* value(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

  int selectedForTest() const { return _list.selected(); }

private:
  AppServices*   _app = nullptr;
  AppletHost*    _host = nullptr;
  TzPickCallback _cb = nullptr;
  void*          _ctx = nullptr;
  int            _pending = -1;     // configured preselect index
  mutable char   _val[12];
  ListMenu       _list;
  StatusBar      _bar;
};

TimezonePickerApplet& timezonePickerApplet();

}  // namespace mishmesh
