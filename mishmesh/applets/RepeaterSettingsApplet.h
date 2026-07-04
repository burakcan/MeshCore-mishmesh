// mishmesh/applets/RepeaterSettingsApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// Settings hub for a logged-in repeater: a menu of config panels. Reached from the
// management hub's Settings tab. P2a rows push a field panel or the actions panel;
// later-phase rows toast "Coming soon". Host-safe.
class RepeaterSettingsApplet : public Applet {
public:
  RepeaterSettingsApplet() : Applet("Settings") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Embedded-view protocol (called by RepeaterManageApplet as tab host).
  void onShow(AppletContext& ctx);
  // noinline: onRender() delegates here; keeps the body out of the vtable-referenced
  // onRender() so it isn't duplicated in flash (the BLE build is tight).
  int  renderBody(Canvas& c, int x, int y, int w, int h) __attribute__((noinline));

  const uint8_t* targetForTest() const { return _pub; }

private:
  AppletHost* _host = nullptr;
  ListMenu    _menu;
  uint8_t _pub[6] = {0};
  char    _name[32] = {0};
};

RepeaterSettingsApplet& repeaterSettingsApplet();

}  // namespace mishmesh
