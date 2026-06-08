#include <mishmesh/applets/BluetoothApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const int REFRESH_MS = 1000;   // status can change when a client connects

void BluetoothApplet::onStart(AppletContext& ctx) {
  _app  = ctx.app;
  _host = ctx.host;
}

int BluetoothApplet::onRender(Canvas& c) {
  bool enabled   = _app && _app->bleEnabled();
  bool connected = _app && _app->bleConnected();
  uint32_t pin   = _app ? _app->blePin() : 0;

  const int W = c.width();
  const int cx = W / 2;
  const DisplayDriver::Color fg = DisplayDriver::LIGHT;

  // Icon (12px Pixelarticons glyph), centred near the top.
  c.drawGlyph(iconFont(), cx - 6, 4, (uint16_t)Icon::Bluetooth, fg);

  // Status word — the connection state (radio on/off is the Toggle below).
  c.drawText(fontBody(), cx, 20, statusText(connected), fg, TextAlign::Center);

  // ON/OFF pill, centred.
  const int pw = 40, ph = 15, px = cx - pw / 2, py = 32;
  _toggle.setOn(enabled);
  _toggle.draw(c, px, py, pw, ph);

  // Read-only pairing PIN (hidden once paired / when unsupported).
  if (showPin(pin, connected)) {
    char line[20];
    snprintf(line, sizeof(line), "Pin: %u", (unsigned)pin);
    c.drawText(fontBody(), cx, py + ph + 4, line, fg, TextAlign::Center);
  }
  return REFRESH_MS;
}

bool BluetoothApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select) {
    if (_app) {
      bool now = !_app->bleEnabled();
      _app->setBleEnabled(now);
      if (_host) _host->postToast(now ? "Bluetooth on" : "Bluetooth off");
    }
    return true;
  }
  return false;   // Back (and everything else) bubbles to the host
}

#ifdef BLE_PIN_CODE
static BluetoothApplet s_bluetooth;
MISHMESH_REGISTER_APPLET_ICON(&s_bluetooth, ::mishmesh::Placement::AppMenu,
                              "Bluetooth", 4, (uint16_t)::mishmesh::Icon::Bluetooth);
#endif

}  // namespace mishmesh
