#include <mishmesh/applets/game2048/Game2048Applet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/applets/game2048/game/Game2048.h>

namespace mishmesh {

void Game2048Applet::onStart(AppletContext& ctx) {
  _runtime.setFrameInterval(33);        // ~30 fps: match the game's frame-counted animations
  _runtime.begin(ctx, "2048");
  game2048Setup();
  _confirming = false;
  _confirm.reset();
}

int Game2048Applet::onRender(Canvas& c) {
  _runtime.setCanvas(c);
  if (!_confirming && _runtime.stepDue(c.now())) {
    _runtime.pumpButtons();
    game2048LoopStep();
  }
  _runtime.present();                    // blit the game framebuffer
  if (_confirming) _confirm.draw(c, 0, 0, c.width(), c.height());
  return 0;
}

bool Game2048Applet::onInput(InputEvent ev) {
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r == ConfirmResult::Confirmed) game2048NewGame();
      if (r != ConfirmResult::None) { _confirming = false; _confirm.reset(); }
    }
    return true;                         // modal: swallow everything while open
  }
  if (ev == InputEvent::Select) {        // center click -> restart (with confirm)
    _confirm.configure("Restart?");
    _confirming = true;
    return true;
  }
  if (ev == InputEvent::Back) {          // let the host pop to the app menu (onStop persists)
    return false;
  }
  return false;                          // directions reach the game via pumpButtons()
}

void Game2048Applet::onStop() {
  game2048Persist();          // capture the current board so re-entering resumes it
  _runtime.saveIfDirty();     // flush the EEPROM image to storage
}

static Game2048Applet s_game2048;
MISHMESH_REGISTER_APPLET_ICON(&s_game2048, ::mishmesh::Placement::AppMenu,
                              "2048", 8, (uint16_t)::mishmesh::Icon::Grid);

}  // namespace mishmesh
