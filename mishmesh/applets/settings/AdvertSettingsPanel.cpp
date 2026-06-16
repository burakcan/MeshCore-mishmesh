#include <mishmesh/applets/settings/AdvertSettingsPanel.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

void AdvertSettingsPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _model.bind(_app);
  _list.setRowHeight(14);
  _list.setModel(&_model);
}

int AdvertSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool AdvertSettingsPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    if (_app) _app->setShareLocationInAdvert(!_app->shareLocationInAdvert());
    return true;
  }
  return false;   // Back bubbles to the host
}

AdvertSettingsPanel& advertSettings() {
  static AdvertSettingsPanel s;
  return s;
}

}  // namespace mishmesh
