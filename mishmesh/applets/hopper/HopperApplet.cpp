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
    if (hopperInGame()) {
      hopperReturnToTitle();  // playing -> drop to Hopper's own title screen, stay in the app
      return true;
    }
    return false;  // at the title/logo -> let the host pop this applet to the mishmesh menu
  }
  return false;
}

static HopperApplet s_hopper;
MISHMESH_REGISTER_APPLET_ICON(&s_hopper, ::mishmesh::Placement::AppMenu,
                              "Hopper", 7, (uint16_t)::mishmesh::Icon::Star);

}  // namespace mishmesh
