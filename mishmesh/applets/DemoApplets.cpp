// Placeholder app-menu entries to preview menu variety, icons, and scrolling.
// Remove (or replace with real feature applets) once those land.
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

class PlaceholderApplet : public Applet {
public:
  explicit PlaceholderApplet(const char* n) : Applet(n) {}
  int onRender(Canvas& c) override {
    int th = c.fontHeight(fontTitle());
    c.drawText(fontTitle(), c.width() / 2, c.height() / 2 - th, name(),
               DisplayDriver::LIGHT, TextAlign::Center);
    c.drawText(fontBody(), c.width() / 2, c.height() / 2 + 4, "demo screen",
               DisplayDriver::LIGHT, TextAlign::Center);
    return 2000;
  }
};

static PlaceholderApplet s_messages("Messages");
static PlaceholderApplet s_settings("Settings");
static PlaceholderApplet s_gps("GPS");
static PlaceholderApplet s_node("Node Info");

MISHMESH_REGISTER_APPLET_ICON(&s_messages, Placement::AppMenu, "Messages", 1, (uint16_t)Icon::Message);
MISHMESH_REGISTER_APPLET_ICON(&s_settings, Placement::AppMenu, "Settings", 3, (uint16_t)Icon::Menu);  // Icon::Settings (cog) renders poorly at 12px
MISHMESH_REGISTER_APPLET_ICON(&s_gps,      Placement::AppMenu, "GPS",      4, (uint16_t)Icon::Gps);
MISHMESH_REGISTER_APPLET_ICON(&s_node,     Placement::AppMenu, "Node Info", 5, (uint16_t)Icon::Home);

}  // namespace mishmesh
