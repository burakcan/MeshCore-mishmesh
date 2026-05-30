#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

// contactDetailApplet() is declared by ContactDetailApplet.h (Task 11); including
// that header gives the complete type needed for setTarget() + the Applet* upcast.

static uint16_t kindIcon(ContactKind k) {
  switch (k) {
    case ContactKind::Chat:     return (uint16_t)Icon::User;
    case ContactKind::Repeater: return (uint16_t)Icon::Radio;
    case ContactKind::Room:     return (uint16_t)Icon::Comment;
    default:                    return (uint16_t)Icon::Chip;
  }
}

// Tab-bar icon for a kind (plural "Users" for the contacts tab vs the per-row User).
static uint16_t tabIcon(ContactKind k) {
  return k == ContactKind::Chat ? (uint16_t)Icon::Users : kindIcon(k);
}

// Row label shared by the per-kind and favourites lists: repeaters get a 2-hex
// key prefix (they often share generic names); everything else is just the name.
static const char* contactLabel(const ContactView& v, char* buf, int n) {
  if (v.type == (uint8_t)ContactKind::Repeater && v.pubKey) {
    snprintf(buf, n, "%02X %s", v.pubKey[0], v.name);
    return buf;
  }
  return v.name;
}

const char* ContactListModel::label(int i) const {
  static ContactView v;
  static char buf[44];
  if (!_svc || !_svc->getByKind(_kind, i, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}

const char* FavouritesListModel::label(int i) const {
  static ContactView v;
  static char buf[44];
  if (!_svc || !_svc->getFavourite(i, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}
uint16_t FavouritesListModel::icon(int i) const {
  static ContactView v;
  if (!_svc || !_svc->getFavourite(i, v)) return 0;
  return kindIcon((ContactKind)v.type);
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

void ContactsApplet::syncListToTab() {
  const TabSlot& s = currentSlot();
  switch (s.kind) {
    case TabKind::Favourites: _list.setModel(&_favs); break;
    case TabKind::Settings:   _list.setModel(&_settings); break;
    default:                  _list.setModel(&_models[(int)s.contactKind - 1]); break;  // kinds are 1-based
  }
}

void ContactsApplet::rebuildTabs() {
  // Preserve what the user was viewing - the Favourites tab can appear or vanish
  // between visits, shifting indices.
  TabKind prevKind = TabKind::Kind; ContactKind prevContact = ContactKind::Chat;
  if (_slotCount > 0) { const TabSlot& s = currentSlot(); prevKind = s.kind; prevContact = s.contactKind; }

  _tabs.clear();   // resets selection to 0
  _slotCount = 0;
  if (_svc && _svc->countFavourites() > 0) {
    _tabs.addTab("Favorites", (uint16_t)Icon::Star);
    _slots[_slotCount++] = {TabKind::Favourites, ContactKind::Chat};
  }
  static const ContactKind KINDS[4] = {
    ContactKind::Chat, ContactKind::Repeater, ContactKind::Room, ContactKind::Sensor };
  static const char* KIND_LABELS[4] = { "Contacts", "Repeaters", "Rooms", "Sensors" };
  for (int i = 0; i < 4; i++) {
    _tabs.addTab(KIND_LABELS[i], tabIcon(KINDS[i]));
    _slots[_slotCount++] = {TabKind::Kind, KINDS[i]};
  }
  _tabs.addTab("Settings", (uint16_t)Icon::Settings);
  _slots[_slotCount++] = {TabKind::Settings, ContactKind::Chat};

  for (int i = 0; i < _slotCount; i++) {
    if (_slots[i].kind != prevKind) continue;
    if (prevKind == TabKind::Kind && _slots[i].contactKind != prevContact) continue;
    _tabs.setSelected(i); break;
  }
  syncListToTab();
}

void ContactsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _models[0].bind(_svc, ContactKind::Chat, kindIcon(ContactKind::Chat));
  _models[1].bind(_svc, ContactKind::Repeater, kindIcon(ContactKind::Repeater));
  _models[2].bind(_svc, ContactKind::Room, kindIcon(ContactKind::Room));
  _models[3].bind(_svc, ContactKind::Sensor, kindIcon(ContactKind::Sensor));
  _favs.bind(_svc);
  _settings.bind(_svc);
  _list.setRowHeight(14);
  _confirming = false; _pendingAction = -1;
  _slotCount = 0;        // fresh build: no selection to preserve
  rebuildTabs();
  // On a fresh open, land on the Favourites tab when it exists (always slot 0).
  if (_slotCount > 0 && _slots[0].kind == TabKind::Favourites) {
    _tabs.setSelected(0);
    syncListToTab();
  }
  _list.resetSelection();   // fresh open starts at the top; onForeground (back) keeps position
}

// Favourites can change while a contact detail is open; refresh the tab set when
// we become the top applet again.
void ContactsApplet::onForeground() { rebuildTabs(); }

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
    // Favourites or a kind tab: drill into detail.
    if (_svc) {
      const TabSlot& s = currentSlot();
      ContactView v;
      bool ok = (s.kind == TabKind::Favourites) ? _svc->getFavourite(_list.selected(), v)
                                                : _svc->getByKind(s.contactKind, _list.selected(), v);
      if (ok) {
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
