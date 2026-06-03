#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/Toggle.h>

namespace mishmesh {

// BLE status / on-off toggle / read-only pairing PIN. Registered only on BLE
// builds (see BluetoothApplet.cpp); reads state via AppServices BLE accessors.
class BluetoothApplet : public Applet {
  AppServices* _app  = nullptr;
  AppletHost*  _host = nullptr;
  Toggle       _toggle;
public:
  BluetoothApplet() : Applet("Bluetooth") {}

  // Pure helpers (host-testable without rendering). Status reflects the link
  // (Connected/Not connected); the radio on/off state is shown by the Toggle.
  static const char* statusText(bool connected) {
    return connected ? "Connected" : "Not connected";
  }
  static bool showPin(uint32_t pin, bool connected) { return pin != 0 && !connected; }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
