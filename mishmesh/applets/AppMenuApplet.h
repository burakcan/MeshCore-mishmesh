#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/widgets/IconGrid.h>

namespace mishmesh {

// App drawer: an icon grid of every Placement::AppMenu registration; Select
// pushes the focused one. LaunchOnly itself (the adapter pushes it from Home).
class AppMenuApplet : public Applet, public ListModel {
  static const int MAX_ENTRIES = 16;
  const AppletRegistration* _entries[MAX_ENTRIES];
  int _count;
  IconGrid _grid;
  AppletHost* _host;
  // [mishmesh] unread badge: pointer to messages service, set in onStart
  struct MessagesService* _ctx_messages = nullptr;
  // [/mishmesh]
public:
  AppMenuApplet() : Applet("Apps"), _count(0), _host(nullptr) {}

  void onStart(AppletContext& ctx) override;
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int count() const override { return _count; }
  const char* label(int i) const override { return _entries[i]->label; }
  uint16_t icon(int i) const override { return _entries[i]->icon; }
  // [mishmesh]
  const char* value(int i) const override;
  // [/mishmesh]
};

}  // namespace mishmesh
