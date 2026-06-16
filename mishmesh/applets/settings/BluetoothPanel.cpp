#include <mishmesh/applets/settings/BluetoothPanel.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>

namespace mishmesh {

const char* BluetoothPanel::Model::label(int i) const {
  switch (i) {
    case Toggle: return "Bluetooth";
    case Status: return "Status";
    default:     return "Pin";
  }
}

const char* BluetoothPanel::Model::value(int i) const {
  if (!app) return nullptr;
  if (i == Status) return statusText(app->bleConnected());
  if (i == Pin) {
    static char buf[12];
    snprintf(buf, sizeof(buf), "%u", (unsigned)app->blePin());
    return buf;
  }
  return nullptr;
}

void BluetoothPanel::begin(AppletContext& ctx) {
  _app  = ctx.app;
  _host = ctx.host;
  _model.app = _app;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();   // singleton reuse: setModel skips reset on same-ptr rebind
}

int BluetoothPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool BluetoothPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    if (_list.selected() == Toggle && _app) {
      bool now = !_app->bleEnabled();
      _app->setBleEnabled(now);
      if (_host) _host->postToast(now ? "Bluetooth on" : "Bluetooth off");
    }
    return true;   // Status/Pin rows are read-only; swallow Select
  }
  return false;   // Back bubbles
}

BluetoothPanel& bluetoothSettings() {
  static BluetoothPanel s;
  return s;
}

}  // namespace mishmesh
