#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>

namespace mishmesh {

// App-menu screen with two tabs. "Advert": a two-row mode picker (Zero hop /
// Flood routed) that broadcasts a self-advert via AppServices::sendAdvert and
// stays on screen. "Recent": adverts heard recently (incl. contacts, newest
// first); selecting a row drills into the contact detail (if a contact) or the
// discover detail (if not). Back bubbles to the host, which pops to the app menu.
class AdvertApplet : public Applet {
public:
  // Send tab: two fixed rows with a mode icon each.
  class SendModel : public ListModel {
  public:
    int count() const override { return 2; }
    const char* label(int index) const override;
    uint16_t icon(int index) const override;
  };
  // Recent tab: forwards to the ContactsService recent-advert feed.
  class RecentModel : public ListModel {
    ContactsService* _svc = nullptr;
    AppServices*     _app = nullptr;
  public:
    void bind(ContactsService* svc, AppServices* app) { _svc = svc; _app = app; }
    int count() const override;
    const char* label(int index) const override;
    uint16_t icon(int index) const override;
    const char* value(int index) const override;   // right-aligned "heard N ago"
  };

  AdvertApplet() : Applet("Advert") {}

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  RecentModel& recentModel() { return _recent; }   // for host tests

private:
  AppServices*     _app  = nullptr;
  ContactsService* _svc  = nullptr;
  AppletHost*      _host = nullptr;
  TabBar           _tabs;
  ListMenu         _list;
  SendModel        _send;
  RecentModel      _recent;
  char             _battBuf[8] = {0};
  void syncListToTab();
  bool settingsTab() const { return _tabs.selected() == 2; }
};

}  // namespace mishmesh
