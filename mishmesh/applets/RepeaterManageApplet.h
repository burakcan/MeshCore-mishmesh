// mishmesh/applets/RepeaterManageApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/TabBar.h>

namespace mishmesh {

// Management hub for a logged-in repeater: a TabBar host with Status, Command line,
// and Settings tabs. The three views are embedded (not pushed on the stack) - they
// render into the body region below the tab bar. Drill-ins (keypad, settings panels)
// still push normally via the host. Reached solely via ContactDetailApplet's Manage
// action (through ServerLoginApplet), so it is not registered in the app menu.
// Host-safe (no Arduino deps).
class RepeaterManageApplet : public Applet {
public:
  RepeaterManageApplet() : Applet("Repeater") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  const uint8_t* targetForTest() const { return _pub; }
  int  selectedTabForTest() const { return _tab; }

private:
  AppletHost*   _host = nullptr;
  AppServices*  _app  = nullptr;
  AppletContext _ctxStore;          // copy of the context for lifetime of the hub
  AppletContext* _ctx = nullptr;    // points to _ctxStore once onStart fires
  TabBar        _tabs;
  int           _tab = 0;
  uint8_t _pub[6] = {0};
  char    _name[32] = {0};

  void activateTab(int t);          // set _tab + call onShow on the new active view
};

RepeaterManageApplet& repeaterManageApplet();

}  // namespace mishmesh
