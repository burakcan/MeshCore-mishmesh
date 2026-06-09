#include <mishmesh/applets/hopper/HopperApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/applets/hopper/game/HopperGame.h>

namespace mishmesh {

void HopperApplet::onStart(AppletContext& ctx) {
  _runtime.begin(ctx, "hopper");
  hopperSetup();
}

int HopperApplet::onRender(Canvas& c) {
  _runtime.setCanvas(c);
  if (_runtime.stepDue(c.now())) {
    _runtime.pumpButtons();
    hopperLoopStep();
  }
  _runtime.present();
  return 0;
}

bool HopperApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select || ev == InputEvent::SelectLong) {
    _runtime.onButtonEvent(ev);
    return true;
  }
  if (ev == InputEvent::Back) {
    _runtime.saveIfDirty();
    if (hopperHandleBack()) {
      return true;   // Hopper consumed Back (unwound one level), stay in the app
    }
    return false;    // at Hopper's top (title menu / logo) -> let the host pop to the mishmesh menu
  }
  return false;
}

static HopperApplet s_hopper;
MISHMESH_REGISTER_APPLET_ICON(&s_hopper, ::mishmesh::Placement::AppMenu,
                              "Hopper", 7, (uint16_t)::mishmesh::Icon::Star);

}  // namespace mishmesh
