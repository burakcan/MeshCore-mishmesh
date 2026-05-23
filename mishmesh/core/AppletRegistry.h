#pragma once

#include <stdint.h>

namespace mishmesh {

class Applet;

enum class Placement : uint8_t {
  AppMenu,      // appears in the app menu
  LaunchOnly,   // launched programmatically, not listed
};

struct AppletRegistration {
  Applet*             applet;
  const char*         label;
  const uint8_t*      icon;       // optional XBM, may be null
  Placement           placement;
  int16_t             order;
  AppletRegistration* next;
};

void registerApplet(AppletRegistration* reg);
AppletRegistration* registeredApplets();
void resetRegistry();

#define MISHMESH_CONCAT_(a, b) a##b
#define MISHMESH_CONCAT(a, b) MISHMESH_CONCAT_(a, b)

// Statically registers an applet so menus can discover it without host wiring.
#define MISHMESH_REGISTER_APPLET(applet_ptr, placement, label, order)            \
  static ::mishmesh::AppletRegistration MISHMESH_CONCAT(_mishmesh_reg_, __LINE__) \
      = {(applet_ptr), (label), nullptr, (placement), (int16_t)(order), nullptr}; \
  static const bool MISHMESH_CONCAT(_mishmesh_reg_done_, __LINE__) =              \
      (::mishmesh::registerApplet(&MISHMESH_CONCAT(_mishmesh_reg_, __LINE__)), true)

}  // namespace mishmesh
