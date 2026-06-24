#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/TabBar.h>

namespace mishmesh {

class Canvas;

// Radio airtime / duty-cycle usage. Two tabs:
//   Budget  - live TX duty-cycle headroom (token bucket) + lifetime totals.
//   History - per-minute TX (solid) + RX (stippled) bars over the last hour,
//             with a dashed line at the sustainable per-minute TX budget.
// Read-only; Back pops. App-menu singleton.
class AirtimeApplet : public Applet {
public:
  AirtimeApplet();

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  void selectTabForTest(int i) { _tabs.setSelected(i); _tab = i; }

private:
  enum { TAB_BUDGET = 0, TAB_HISTORY = 1 };
  int  renderBudget(Canvas& c, int y, int h);
  int  renderHistory(Canvas& c, int y, int h);

  AppServices* _app = nullptr;
  AirtimeStats _st;          // refreshed each onRender
  TabBar _tabs;
  int    _tab = 0;
};

AirtimeApplet& airtimeApplet();

}  // namespace mishmesh
