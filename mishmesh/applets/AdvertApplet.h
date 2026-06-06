#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// App-menu screen that broadcasts a self-advert. Two rows: "Zero hop"
// (neighbours only) and "Flood routed" (whole mesh). Select fires immediately
// via AppServices::sendAdvert and stays on-screen so the user can resend or
// switch modes; Back bubbles to the host, which pops back to the app menu.
class AdvertApplet : public Applet, public ListModel {
  AppServices* _app  = nullptr;
  AppletHost*  _host = nullptr;
  ListMenu     _list;
public:
  AdvertApplet() : Applet("Advert") {}

  // ListModel — two fixed rows.
  int count() const override { return 2; }
  const char* label(int index) const override;

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
