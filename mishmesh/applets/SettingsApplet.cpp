#include <mishmesh/applets/SettingsApplet.h>
#include <mishmesh/applets/SettingsDetailApplet.h>
#include <mishmesh/applets/settings/ContactsSettingsPanel.h>
#include <mishmesh/applets/settings/MessagesSettingsPanel.h>
#include <mishmesh/applets/settings/AdvertSettingsPanel.h>
#include <mishmesh/applets/settings/SystemInfoPanel.h>
#include <mishmesh/applets/settings/BluetoothPanel.h>
#include <mishmesh/applets/settings/SoundPanel.h>
#include <mishmesh/applets/settings/RadioSettingsPanel.h>
#include <mishmesh/applets/settings/TimeSettingsPanel.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

static SettingsPanel* contactsPanelPtr()    { return &contactsSettings(); }
static SettingsPanel* messagesPanelPtr()    { return &messagesSettings(); }
static SettingsPanel* advertPanelPtr()      { return &advertSettings(); }
static SettingsPanel* systemInfoPanelPtr()  { return &systemInfoSettings(); }
static SettingsPanel* bluetoothPanelPtr()   { return &bluetoothSettings(); }
static SettingsPanel* soundPanelPtr()       { return &soundSettings(); }
static SettingsPanel* radioPanelPtr()       { return &radioSettings(); }
static SettingsPanel* timePanelPtr()        { return &timeSettings(); }

static bool always(const AppletContext&) { return true; }
static bool bleAvail(const AppletContext& c) { return c.app && c.app->bleSupported(); }
static bool soundAvail(const AppletContext& c) { return c.sound != nullptr; }

const SettingsApplet::Entry SettingsApplet::ENTRIES[ENTRY_COUNT] = {
  { "Contacts",    contactsPanelPtr,   (uint16_t)Icon::Users,     always    },
  { "Messages",    messagesPanelPtr,   (uint16_t)Icon::Message,   always    },
  { "Advert",      advertPanelPtr,     (uint16_t)Icon::Radio,     always    },
  { "Radio",       radioPanelPtr,      (uint16_t)Icon::Wifi,      always    },
  { "Time & date", timePanelPtr,       (uint16_t)Icon::Clock,     always    },
  { "Bluetooth",   bluetoothPanelPtr,  (uint16_t)Icon::Bluetooth, bleAvail  },
  { "Sound",       soundPanelPtr,      (uint16_t)Icon::Bell,      soundAvail },
  { "System Info", systemInfoPanelPtr, (uint16_t)Icon::Chip,      always    },
};

int SettingsApplet::Model::count() const { return owner ? owner->_visibleCount : 0; }
const char* SettingsApplet::Model::label(int i) const {
  if (!owner || i < 0 || i >= owner->_visibleCount) return "";
  return ENTRIES[owner->_visible[i]].label;
}
uint16_t SettingsApplet::Model::icon(int i) const {
  if (!owner || i < 0 || i >= owner->_visibleCount) return 0;
  return ENTRIES[owner->_visible[i]].icon;
}

void SettingsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app  = ctx.app;
  _bar.setTitle("Settings");
  _visibleCount = 0;
  for (int i = 0; i < ENTRY_COUNT; i++) {
    if (ENTRIES[i].available(ctx)) _visible[_visibleCount++] = i;
  }
  _model.owner = this;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
}

int SettingsApplet::onRender(Canvas& c) {
  int bw = 0, bh = 0; _bar.measure(bw, bh);
  _bar.setBattery(_app ? _app->batteryMillivolts() : 0);
  _bar.draw(c, 0, 0, c.width(), bh);
  _list.draw(c, 0, bh + 1, c.width(), c.height() - (bh + 1));
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool SettingsApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _host) {
    int vi = _list.selected();
    if (vi >= 0 && vi < _visibleCount) {
      const Entry& e = ENTRIES[_visible[vi]];
      settingsDetailApplet().setPanel(e.panel());
      _host->push(&settingsDetailApplet());
    }
    return true;
  }
  return false;   // Back bubbles -> host pops to the app menu
}

static SettingsApplet s_settings;
MISHMESH_REGISTER_APPLET_ICON(&s_settings, ::mishmesh::Placement::AppMenu,
                              "Settings", 9, (uint16_t)::mishmesh::Icon::Settings);

}  // namespace mishmesh
