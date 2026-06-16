#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// BLE settings as rows: an on/off toggle, a read-only link Status, and a PIN
// row shown only while a pairing PIN is relevant. State lives in AppServices.
class BluetoothPanel : public SettingsPanel {
public:
  // Pure helpers (host-testable). Status reflects the link; the radio on/off
  // state is the toggle. PIN is shown only when present and not yet paired.
  static const char* statusText(bool connected) { return connected ? "Connected" : "Not connected"; }
  static bool showPin(uint32_t pin, bool connected) { return pin != 0 && !connected; }

  const char* title() const override { return "Bluetooth"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

private:
  enum Row { Toggle, Status, Pin, ROW_COUNT };
  struct Model : ListModel {
    AppServices* app = nullptr;
    bool pinVisible() const { return app && showPin(app->blePin(), app->bleConnected()); }
    int count() const override { return pinVisible() ? 3 : 2; }
    const char* label(int i) const override;
    bool isToggle(int i) const override { return i == Toggle; }
    bool toggleState(int i) const override { return app && app->bleEnabled(); }
    const char* value(int i) const override;   // Status / Pin
  } _model;

  AppServices* _app  = nullptr;
  AppletHost*  _host = nullptr;
  ListMenu     _list;
};

BluetoothPanel& bluetoothSettings();   // shared singleton

}  // namespace mishmesh
