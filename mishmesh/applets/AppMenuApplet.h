#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// App drawer: lists every Placement::AppMenu registration and pushes the chosen
// one. LaunchOnly itself (the adapter pushes it from Home).
class AppMenuApplet : public Applet, public ListModel {
  static const int MAX_ENTRIES = 16;
  const AppletRegistration* _entries[MAX_ENTRIES];
  int _count;
  ListMenu _list;
  AppletHost* _host;
public:
  AppMenuApplet() : Applet("Apps"), _count(0), _host(nullptr) {}

  void onStart(AppletContext& ctx) override;
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int count() const override { return _count; }
  const char* label(int i) const override { return _entries[i]->label; }
};

}  // namespace mishmesh
