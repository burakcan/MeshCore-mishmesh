#pragma once

#include <stdint.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletRegistry.h>

namespace mishmesh {

class NavigationApplet : public Applet {
  static const uint8_t MAX_WAYPOINTS = 4;
  static const uint8_t LABEL_LEN = 8;

  struct Waypoint {
    int32_t lat;
    int32_t lon;
    char label[LABEL_LEN];
  };

  AppletContext* _ctx = nullptr;
  Waypoint _wp[MAX_WAYPOINTS]{};
  uint8_t _count = 0;
  int8_t _sel = 0;
  int8_t _nav = -1;
  bool _dirty = false;

  void load();
  void save();
  bool ownLocation(int32_t& lat, int32_t& lon) const;
  void markHere();
  void renameWaypoint();
  void deleteWaypoint();
  static void onRenameDone(void* ctx, const char* text);
  void renderList(Canvas& c);
  void renderNav(Canvas& c);
  static float haversineKm(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);
  static int bearingDeg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);
  static const char* cardinal(int deg);
  static void formatDistance(char* out, int cap, float km);

public:
  NavigationApplet() : Applet("Navigation") {}

  void onStart(AppletContext& ctx) override;
  void onStop() override { _ctx = nullptr; _nav = -1; }
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool keepOnWake() const override { return _nav >= 0; }
};

NavigationApplet& navigationApplet();
void registerNavigationApplet();

}  // namespace mishmesh
