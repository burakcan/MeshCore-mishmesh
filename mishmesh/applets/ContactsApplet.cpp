#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

// contactDetailApplet() is declared by ContactDetailApplet.h (Task 11); including
// that header gives the complete type needed for setTarget() + the Applet* upcast.

static uint16_t kindIcon(ContactKind k) {
  switch (k) {
    case ContactKind::Chat:     return (uint16_t)Icon::User;
    case ContactKind::Repeater: return (uint16_t)Icon::Wifi;
    case ContactKind::Room:     return (uint16_t)Icon::Message;
    default:                    return (uint16_t)Icon::Bell;
  }
}

const char* ContactListModel::label(int i) const {
  static ContactView v;
  if (_svc && _svc->getByKind(_kind, i, v)) return v.name;
  return "";
}

static const char* SETTINGS_LABELS[ContactsSettingsModel::ROW_COUNT] = {
  "Auto-add Users", "Auto-add Repeaters", "Auto-add Rooms", "Auto-add Sensors",
  "Overwrite oldest", "Remove non-users", "Remove all contacts",
};
const char* ContactsSettingsModel::label(int i) const {
  return (i >= 0 && i < ROW_COUNT) ? SETTINGS_LABELS[i] : "";
}
bool ContactsSettingsModel::toggleState(int i) const {
  if (!_svc) return false;
  AutoAddConfig c = _svc->getAutoAdd();
  switch (i) {
    case Users:     return c.addChat;
    case Repeaters: return c.addRepeater;
    case Rooms:     return c.addRoom;
    case Sensors:   return c.addSensor;
    case Overwrite: return c.overwriteOldest;
    default:        return false;
  }
}

ContactsApplet::ContactsApplet()
    : Applet("Contacts"), _host(nullptr), _svc(nullptr),
      _confirming(false), _pendingAction(-1) {}

ContactKind ContactsApplet::currentKind() const {
  static const ContactKind kinds[4] = {
    ContactKind::Chat, ContactKind::Repeater, ContactKind::Room, ContactKind::Sensor };
  int t = _tabs.selected();
  return kinds[t < 4 ? t : 0];
}

void ContactsApplet::syncListToTab() {
  if (settingsTab()) { _list.setModel(&_settings); }
  else { _list.setModel(&_models[_tabs.selected()]); }
}

void ContactsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _tabs.clear();
  _tabs.addTab("Cont", (uint16_t)Icon::Users);
  _tabs.addTab("Rep");
  _tabs.addTab("Room");
  _tabs.addTab("Sens");
  _tabs.addTab("Set");   // settings (cog glyph renders poorly at 12px; use text)
  _models[0].bind(_svc, ContactKind::Chat, kindIcon(ContactKind::Chat));
  _models[1].bind(_svc, ContactKind::Repeater, kindIcon(ContactKind::Repeater));
  _models[2].bind(_svc, ContactKind::Room, kindIcon(ContactKind::Room));
  _models[3].bind(_svc, ContactKind::Sensor, kindIcon(ContactKind::Sensor));
  _settings.bind(_svc);
  _list.setRowHeight(14);
  syncListToTab();
  _confirming = false; _pendingAction = -1;
}

int ContactsApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int barH = 13;
  _tabs.draw(c, 0, 0, w, barH);
  int bodyY = barH + 1;
  int bodyH = h - bodyY;
  _list.draw(c, 0, bodyY, w, bodyH);   // contacts and settings share the one list widget

  if (_confirming) { _confirm.draw(c, 0, 0, w, h); return 100; }
  return _list.needsAnimation() ? 90 : 500;
}

bool ContactsApplet::onInput(InputEvent ev) {
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _svc) {
          if (_pendingAction == ContactsSettingsModel::RemoveNonUsers) _svc->removeNonChat();
          else if (_pendingAction == ContactsSettingsModel::RemoveAll) _svc->removeAll();
        }
        _confirming = false; _pendingAction = -1;
      }
      return true;
    }
    return true;   // swallow everything while modal
  }

  if (_tabs.onInput(ev)) { syncListToTab(); return true; }
  if (_list.onInput(ev)) return true;

  if (ev == InputEvent::Select) {
    if (settingsTab()) {
      int i = _list.selected();
      if (_settings.isToggle(i) && _svc) {
        AutoAddConfig cfg = _svc->getAutoAdd();
        switch (i) {
          case ContactsSettingsModel::Users:     cfg.addChat = !cfg.addChat; break;
          case ContactsSettingsModel::Repeaters: cfg.addRepeater = !cfg.addRepeater; break;
          case ContactsSettingsModel::Rooms:     cfg.addRoom = !cfg.addRoom; break;
          case ContactsSettingsModel::Sensors:   cfg.addSensor = !cfg.addSensor; break;
          case ContactsSettingsModel::Overwrite: cfg.overwriteOldest = !cfg.overwriteOldest; break;
        }
        _svc->setAutoAdd(cfg);
      } else {
        _pendingAction = i;
        _confirm.configure(i == ContactsSettingsModel::RemoveAll ? "Remove ALL contacts?"
                                                                 : "Remove non-user contacts?");
        _confirming = true;
      }
      return true;
    }
    // Contact tab: drill into detail.
    if (_svc) {
      ContactView v;
      if (_svc->getByKind(currentKind(), _list.selected(), v)) {
        contactDetailApplet().setTarget(v.pubKey);
        if (_host) _host->push(&contactDetailApplet());
      }
    }
    return true;
  }
  return false;   // Back bubbles -> host pops back to app menu
}

ContactsApplet& contactsApplet() {
  static ContactsApplet s_contacts;
  return s_contacts;
}

MISHMESH_REGISTER_APPLET_ICON(&contactsApplet(), Placement::AppMenu, "Contacts", 2,
                              (uint16_t)Icon::Users);

}  // namespace mishmesh
