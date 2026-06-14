#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Per-contact telemetry permissions: a 3-row toggle list reached from the
// contact detail menu. Each Select flips one TelemetryPerm bit on the target
// contact (persisted by the service). Launch-only, like ContactDetailApplet.
class ContactPermissionsApplet : public Applet, public ListModel {
public:
  enum Row { AllowRequests, IncludeLocation, IncludeSensors, ROW_COUNT };
private:
  AppletHost*      _host;
  AppServices*     _app;
  ContactsService* _svc;
  uint8_t          _pubkey[6];
  char             _name[44];     // display name; shown as the top-bar title
  uint8_t          _perms;        // cached TelemetryPerm mask for the target
  ListMenu         _list;
  StatusBar        _bar;          // top bar: contact name + battery

  static uint8_t bitFor(int row);
public:
  ContactPermissionsApplet();
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  // ListModel (3 toggle rows)
  int count() const override { return ROW_COUNT; }
  const char* label(int i) const override;
  bool isToggle(int) const override { return true; }
  bool toggleState(int i) const override;
};

ContactPermissionsApplet& contactPermissionsApplet();

}  // namespace mishmesh
