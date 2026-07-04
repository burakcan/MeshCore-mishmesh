// mishmesh/applets/RepeaterSettingsApplet.cpp
#include <mishmesh/applets/RepeaterSettingsApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/RepeaterSettingsPanel.h>
#include <mishmesh/applets/RepeaterActionsPanel.h>
#include <mishmesh/applets/RepeaterRadioPanel.h>
#include <mishmesh/applets/RepeaterTelemetryApplet.h>
#include <mishmesh/applets/RepeaterNeighborsApplet.h>
#include <mishmesh/applets/RepeaterRegionsApplet.h>
#include <mishmesh/applets/RepeaterAclApplet.h>
#include <mishmesh/applets/RepeaterIdentityApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

using SF = SettingFieldDef;

// P2a field tables
static const SF PUBLIC_INFO[] = {
  {"Name",       "get name",       "set name", SF::Text, 0, 0},
  {"Public key", "get public.key", nullptr,    SF::ReadOnly, 0, 0, nullptr, true},
  {"Role",       "get role",       nullptr,    SF::ReadOnly, 0, 0},
};
static const SF OWNER[] = {
  {"Owner info", "get owner.info", "set owner.info", SF::Text, 0, 0},
};
static const SF POSITION[] = {
  {"Latitude",  "get lat", "set lat", SF::Float, 0, 0},
  {"Longitude", "get lon", "set lon", SF::Float, 0, 0},
};
static const SF ADVERT[] = {
  {"Zero-hop min", "get advert.interval",       "set advert.interval",       SF::Number, 60, 240},
  {"Flood hr",     "get flood.advert.interval", "set flood.advert.interval", SF::Number, 3, 168},
};
static const SF REPEAT[] = {
  {"Repeat mode", "get repeat", "set repeat", SF::Toggle, 0, 0},
};
static const SF PASSWORDS[] = {
  {"Admin pass", nullptr, "password",           SF::Text, 0, 0, "password now:"},
  {"Guest pass", nullptr, "set guest.password", SF::Text, 0, 0},
};
static const SF VERSION[] = {
  {"Version", "ver", nullptr, SF::ReadOnly, 0, 0},
};

// Catalog: a panel (defs+n+title), the actions applet, or a coming-soon placeholder.
struct Entry { const char* label; const SF* defs; int n; const char* title; uint8_t kind; };
enum { K_PANEL = 0, K_ACTIONS = 1, K_SOON = 2, K_RADIO = 3, K_TELEM = 4, K_NEIGH = 5, K_REGIONS = 6, K_ACL = 7, K_IDENTITY = 8 };
static const Entry CATALOG[] = {
  {"Public info",   PUBLIC_INFO, 3, "Public info", K_PANEL},
  {"Radio",         nullptr,     0, nullptr,        K_RADIO},
  {"Owner info",    OWNER,       1, "Owner",       K_PANEL},
  {"Position",      POSITION,    2, "Position",    K_PANEL},
  {"Advert",        ADVERT,      2, "Advert",      K_PANEL},
  {"Repeat mode",   REPEAT,      1, "Repeat",      K_PANEL},
  {"Passwords",     PASSWORDS,   2, "Passwords",   K_PANEL},
  {"Access control",nullptr,     0, nullptr,        K_ACL},
  {"Change identity",nullptr,    0, nullptr,        K_IDENTITY},
  {"Regions",       nullptr,     0, nullptr,        K_REGIONS},
  {"Telemetry",     nullptr,     0, nullptr,        K_TELEM},
  {"Neighbors",     nullptr,     0, nullptr,        K_NEIGH},
  {"Version",       VERSION,     1, "Version",     K_PANEL},
  {"Actions",       nullptr,     0, nullptr,        K_ACTIONS},
};
static const int CATALOG_N = (int)(sizeof(CATALOG) / sizeof(CATALOG[0]));

namespace {
struct SettingsHubModel : ListModel {
  int count() const override { return CATALOG_N; }
  const char* label(int i) const override { return CATALOG[i].label; }
};
static SettingsHubModel s_hubModel;
}  // namespace

void RepeaterSettingsApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterSettingsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _menu.setModel(&s_hubModel);
  _menu.resetSelection();
}

// Embedded-mode activation: update host reference. Menu state (current selection)
// is preserved across tab switches; onStart resets it on fresh hub open.
void RepeaterSettingsApplet::onShow(AppletContext& ctx) {
  _host = ctx.host;
}

int RepeaterSettingsApplet::onRender(Canvas& c) {
  return renderBody(c, 0, 0, c.width(), c.height());
}

// Embedded-mode render: draws the catalog list into the given sub-region.
int RepeaterSettingsApplet::renderBody(Canvas& c, int x, int y, int w, int h) {
  _menu.draw(c, x, y, w, h);
  return _menu.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool RepeaterSettingsApplet::onInput(InputEvent ev) {
  if (_menu.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    const Entry& e = CATALOG[_menu.selected()];
    if (e.kind == K_PANEL) {
      repeaterSettingsPanel().setTarget(_pub, _name);
      repeaterSettingsPanel().setModel(e.defs, e.n, e.title);
      if (_host) _host->push(&repeaterSettingsPanel());
    } else if (e.kind == K_ACTIONS) {
      repeaterActionsPanel().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterActionsPanel());
    } else if (e.kind == K_RADIO) {
      repeaterRadioPanel().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterRadioPanel());
    } else if (e.kind == K_TELEM) {
      repeaterTelemetryApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterTelemetryApplet());
    } else if (e.kind == K_NEIGH) {
      repeaterNeighborsApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterNeighborsApplet());
    } else if (e.kind == K_REGIONS) {
      repeaterRegionsApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterRegionsApplet());
    } else if (e.kind == K_ACL) {
      repeaterAclApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterAclApplet());
    } else if (e.kind == K_IDENTITY) {
      repeaterIdentityApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterIdentityApplet());
    } else {
      if (_host) _host->postToast("Coming soon");
    }
    return true;
  }
  return false;   // Back bubbles to the management hub
}

RepeaterSettingsApplet& repeaterSettingsApplet() {
  static RepeaterSettingsApplet s_settings;
  return s_settings;
}

}  // namespace mishmesh
