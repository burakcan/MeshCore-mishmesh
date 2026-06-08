#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/arduboy/ArduboyRuntime.h>

namespace mishmesh {

class HopperApplet : public Applet {
public:
  HopperApplet() : Applet("Hopper") {}

  bool wantsExclusive() const override { return true; }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

private:
  ::mishmesh::arduboy::ArduboyRuntime _runtime;
};

}  // namespace mishmesh
