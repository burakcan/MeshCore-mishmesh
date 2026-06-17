#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/arduboy/ArduboyRuntime.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

class Game2048Applet : public Applet {
public:
  Game2048Applet() : Applet("2048") {}

  bool wantsExclusive() const override { return true; }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  void onStop() override;

private:
  ::mishmesh::arduboy::ArduboyRuntime _runtime;
  ConfirmDialog _confirm;
  bool _confirming = false;
};

}  // namespace mishmesh
