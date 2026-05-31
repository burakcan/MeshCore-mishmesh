#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// The root shell: a status bar plus a centred clock. Select opens the app menu,
// which the adapter injects via setMenu() so Home stays unaware of the menu type.
class HomeApplet : public Applet {
  StatusBar _bar;
  Applet* _menu;
  AppletHost* _host;
  AppServices* _app;
  // [mishmesh] total-unread indicator
  struct MessagesService* _msgs = nullptr;
  // [/mishmesh]
public:
  HomeApplet() : Applet("Home"), _menu(nullptr), _host(nullptr), _app(nullptr) {}

  void setMenu(Applet* m) { _menu = m; }

  void onStart(AppletContext& ctx) override;
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
