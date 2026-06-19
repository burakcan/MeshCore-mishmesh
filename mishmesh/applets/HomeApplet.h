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
  AppletHost* _host;
  AppServices* _app;
  sound::SoundEngine* _sound = nullptr;
  struct MessagesService* _msgs = nullptr;
public:
  HomeApplet() : Applet("Home"), _menu(nullptr), _host(nullptr), _app(nullptr) {}

  void setMenu(Applet* m) { _menu = m; }

  // First 4 chars of a registry label, for the hint bar. out must hold 5.
  static void hintLabel(const char* label, char out[5]);

  QuickDrawer& drawerForTest() { return _drawer; }

  void onStart(AppletContext& ctx) override;
  void onBackground() override { _drawer.closeNow(); }
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
