// mishmesh/applets/RepeaterManageApplet.h
#pragma once
#include <mishmesh/core/Applet.h>

namespace mishmesh {

// Placeholder screen shown after a successful repeater login. v1 only confirms
// the logged-in state; management actions attach here later. Reached solely via
// ContactDetailApplet's Manage action (through ServerLoginApplet), so it is not
// registered in the app menu. Host-safe (no Arduino deps).
class RepeaterManageApplet : public Applet {
public:
  RepeaterManageApplet() : Applet("Repeater") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  int  onRender(Canvas& c) override;
  // no onInput override: Back bubbles to the host, which pops us.

  const uint8_t* targetForTest() const { return _pub; }

private:
  uint8_t _pub[6] = {0};
  char    _name[32] = {0};
};

RepeaterManageApplet& repeaterManageApplet();

}  // namespace mishmesh
