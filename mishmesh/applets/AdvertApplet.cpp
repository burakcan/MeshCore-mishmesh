#include <mishmesh/applets/AdvertApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

static const char* const kLabel[2] = { "Zero hop", "Flood routed" };
// One-line description of the highlighted mode, shown as a footer hint.
static const char* const kHint[2]  = { "Neighbours only, no relay",
                                       "Whole mesh, multi-hop" };

const char* AdvertApplet::label(int index) const {
  return (index >= 0 && index < 2) ? kLabel[index] : "";
}

void AdvertApplet::onStart(AppletContext& ctx) {
  _app  = ctx.app;
  _host = ctx.host;
  _list.setRowHeight(14);
  _list.setModel(this);
}

int AdvertApplet::onRender(Canvas& c) {
  const int W = c.width();
  const int H = c.height();
  // Two-row picker at the top; footer hint for the highlighted mode below it.
  _list.draw(c, 0, 0, W, _list.rowHeight() * 2);
  c.drawText(fontBody(), W / 2, H - 12, kHint[_list.selected()],
             DisplayDriver::LIGHT, TextAlign::Center);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool AdvertApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;   // NavUp/NavDown move the selection
  if (ev == InputEvent::Select) {
    bool flood = (_list.selected() == 1);
    bool ok = _app && _app->sendAdvert(flood);
    if (_host) {
      _host->postToast(ok ? (flood ? "Flood advert sent" : "Zero-hop advert sent")
                          : "Advert failed");
    }
    return true;
  }
  return false;   // Back (and everything else) bubbles to the host
}

static AdvertApplet s_advert;
MISHMESH_REGISTER_APPLET_ICON(&s_advert, ::mishmesh::Placement::AppMenu,
                              "Advert", 3, (uint16_t)::mishmesh::Icon::Radio);

}  // namespace mishmesh
