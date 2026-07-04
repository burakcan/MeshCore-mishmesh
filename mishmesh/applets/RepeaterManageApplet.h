// mishmesh/applets/RepeaterManageApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// Management hub for a logged-in repeater: a menu routing to Status, Command line,
// and Settings. Only Command line is live in P0; the others show a "coming soon"
// toast until their screens land. Reached solely via ContactDetailApplet's Manage
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

private:
  AppletHost* _host = nullptr;
  ListMenu    _menu;
  uint8_t _pub[6] = {0};
  char    _name[32] = {0};
};

RepeaterManageApplet& repeaterManageApplet();

}  // namespace mishmesh
