#include <mishmesh/applets/AdvertApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>   // contactDetailApplet()
#include <mishmesh/applets/DiscoverDetailApplet.h>  // discoverDetailApplet()
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactFormat.h>             // kindIcon / contactLabel / contactFormatAge
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/StatusBar.h>              // batteryPercent
#include <stdio.h>

namespace mishmesh {

static const char* const kSendLabel[2] = { "Zero hop", "Flood routed" };

const char* AdvertApplet::SendModel::label(int index) const {
  return (index >= 0 && index < 2) ? kSendLabel[index] : "";
}
uint16_t AdvertApplet::SendModel::icon(int index) const {
  return (uint16_t)(index == 1 ? Icon::Radio : Icon::Wifi);   // 0 = local/zero-hop, 1 = network/flood
}

int AdvertApplet::RecentModel::count() const { return _svc ? _svc->countRecentAdverts() : 0; }

const char* AdvertApplet::RecentModel::label(int index) const {
  static ContactView v;
  static char buf[44];
  if (!_svc || !_svc->getRecentAdvert(index, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}
uint16_t AdvertApplet::RecentModel::icon(int index) const {
  static ContactView v;
  if (!_svc || !_svc->getRecentAdvert(index, v)) return 0;
  return kindIcon((ContactKind)v.type);
}
const char* AdvertApplet::RecentModel::value(int index) const {
  static ContactView v;
  static char buf[12];
  if (!_svc || !_app || !_svc->getRecentAdvert(index, v)) return nullptr;
  uint32_t now = _app->epochSeconds();
  if (now == 0 || v.heardAt == 0 || now < v.heardAt) return nullptr;
  contactFormatAge(now - v.heardAt, buf, sizeof(buf));
  return buf;
}

void AdvertApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _svc = ctx.contacts; _host = ctx.host;
  _recent.bind(_svc, _app);
  _settings.bind(_app);
  _tabs.clear();
  _tabs.addTab("Advert", (uint16_t)Icon::Radio);
  _tabs.addTab("Recent", (uint16_t)Icon::Search);
  _tabs.addTab("Settings", (uint16_t)Icon::Settings);
  _list.setRowHeight(14);
  syncListToTab();
  _list.resetSelection();
}

void AdvertApplet::syncListToTab() {
  switch (_tabs.selected()) {
    case 0:  _list.setModel(&_send);     _list.setEmptyText(nullptr);            break;
    case 1:  _list.setModel(&_recent);   _list.setEmptyText("No adverts yet");   break;
    default: _list.setModel(&_settings); _list.setEmptyText(nullptr);            break;
  }
}

int AdvertApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int barH = 13;
  snprintf(_battBuf, sizeof(_battBuf), "%d%%", batteryPercent(_app ? _app->batteryMillivolts() : 0));
  _tabs.setDecoration(_battBuf);
  _tabs.draw(c, 0, 0, w, barH);
  int bodyY = barH + 1;
  _list.draw(c, 0, bodyY, w, h - bodyY);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool AdvertApplet::onInput(InputEvent ev) {
  if (_tabs.onInput(ev)) { syncListToTab(); return true; }   // NavLeft/Right switch tabs
  if (_list.onInput(ev)) return true;                        // NavUp/Down move the row

  if (ev == InputEvent::Select) {
    if (_tabs.selected() == 0) {                             // Advert (send) tab
      bool flood = (_list.selected() == 1);
      bool ok = _app && _app->sendAdvert(flood);
      if (_host) {
        _host->postToast(ok ? (flood ? "Flood advert sent" : "Zero-hop advert sent")
                            : "Advert failed");
      }
      return true;
    }
    if (_tabs.selected() == 2) {                             // Settings tab
      if (_app) _app->setShareLocationInAdvert(!_app->shareLocationInAdvert());
      return true;
    }
    // Recent tab: route to the contact detail (if a contact) or discover detail.
    if (_svc) {
      ContactView v;
      if (_svc->getRecentAdvert(_list.selected(), v)) {
        if (_svc->isContact(v.pubKey)) {
          contactDetailApplet().setTarget(v.pubKey);
          if (_host) _host->push(&contactDetailApplet());
        } else {
          discoverDetailApplet().setTarget(v);
          if (_host) _host->push(&discoverDetailApplet());
        }
      }
    }
    return true;
  }
  return false;   // Back (and everything else) bubbles to the host
}

static AdvertApplet s_advert;
MISHMESH_REGISTER_APPLET_ICON(&s_advert, ::mishmesh::Placement::AppMenu,
                              "Advert", 3, (uint16_t)::mishmesh::Icon::Radio);

}  // namespace mishmesh
