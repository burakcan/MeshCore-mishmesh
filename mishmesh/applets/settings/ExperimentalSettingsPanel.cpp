#include <mishmesh/applets/settings/ExperimentalSettingsPanel.h>
#include <mishmesh/applets/PathHashApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

const char* ExperimentalSettingsPanel::Model::label(int i) const {
  return i == PathHash ? "Path hash size" : "";
}

const char* ExperimentalSettingsPanel::Model::value(int i) const {
  if (i != PathHash || !app) return nullptr;
  switch (app->pathHashMode()) {
    case 0:  return "1 byte";
    case 1:  return "2 byte";
    case 2:  return "3 byte";
    default: return "1 byte";
  }
}

void ExperimentalSettingsPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _model.app = _app;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
}

int ExperimentalSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool ExperimentalSettingsPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _host && _list.selected() == Model::PathHash) {
    _host->push(&pathHashApplet());
    return true;
  }
  return false;   // Back bubbles
}

ExperimentalSettingsPanel& experimentalSettings() {
  static ExperimentalSettingsPanel s;
  return s;
}

}  // namespace mishmesh
