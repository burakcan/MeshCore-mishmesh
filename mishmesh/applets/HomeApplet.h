#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/BatteryIndicator.h>
#include <mishmesh/widgets/QuickDrawer.h>

namespace mishmesh {

// The root shell, watch-face style: top bar (node name + exceptional-state
// icons + battery), left-aligned clock/date, unread line, and a persistent
// gesture hint bar. Select opens the injected app menu; NavLeft/NavRight
// open the UiPrefs-configured shortcuts; NavDown opens the quick-toggles
// drawer overlay (owned here so the face stays visible beneath it).
class HomeApplet : public Applet {
  BatteryIndicator _batt;
  QuickDrawer _drawer;
  Applet* _menu;
  Applet* _lock = nullptr;
  AppletHost* _host;
  AppServices* _app;
  sound::SoundEngine* _sound = nullptr;
  struct MessagesService* _msgs = nullptr;

  // Triple-Back (three presses in quick succession) locks the screen.
  static const uint32_t LOCK_TAP_WINDOW_MS = 700;   // max gap between the taps
  uint8_t  _backTaps = 0;
  uint32_t _lastBackMs = 0;
public:
  HomeApplet() : Applet("Home"), _menu(nullptr), _host(nullptr), _app(nullptr) {}

  void setMenu(Applet* m) { _menu = m; }
  void setLock(Applet* l) { _lock = l; }

  // First 4 chars of a registry label, for the hint bar. out must hold 5.
  static void hintLabel(const char* label, char out[5]);

  QuickDrawer& drawerForTest() { return _drawer; }

  void onStart(AppletContext& ctx) override;
  void onBackground() override { _drawer.closeNow(); }
  void onSleep() override { _drawer.closeNow(); }
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
