#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/applets/DiscoverDetailApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/MessageStore.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/core/ContactFormat.h>   // kindIcon + contactLabel (shared)
#include <stdio.h>

namespace mishmesh {

// contactDetailApplet() is declared by ContactDetailApplet.h (Task 11); including
// that header gives the complete type needed for setTarget() + the Applet* upcast.

// Tab-bar icon for a kind (plural "Users" for the contacts tab vs the per-row User).
static uint16_t tabIcon(ContactKind k) {
  return k == ContactKind::Chat ? (uint16_t)Icon::Users : kindIcon(k);
}

const char* ContactListModel::label(int i) const {
  static ContactView v;
  static char buf[44];
  if (!_svc || !_svc->getByKind(_kind, i, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}

int FavouritesListModel::count() const {
  if (!_svc) return 0;
  if (!_usersOnly) return _svc->countFavourites();
  int n = 0; ContactView v;
  for (int i = 0; i < _svc->countFavourites(); i++)
    if (_svc->getFavourite(i, v) && v.type == (uint8_t)ContactKind::Chat) n++;
  return n;
}
int FavouritesListModel::underlying(int filtered) const {
  if (!_svc) return -1;
  if (!_usersOnly) return filtered;
  int seen = 0; ContactView v;
  for (int i = 0; i < _svc->countFavourites(); i++) {
    if (_svc->getFavourite(i, v) && v.type == (uint8_t)ContactKind::Chat) {
      if (seen == filtered) return i;
      seen++;
    }
  }
  return -1;
}
const char* FavouritesListModel::label(int i) const {
  static ContactView v;
  static char buf[44];
  int u = underlying(i);
  if (u < 0 || !_svc || !_svc->getFavourite(u, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}
uint16_t FavouritesListModel::icon(int i) const {
  static ContactView v;
  int u = underlying(i);
  if (u < 0 || !_svc || !_svc->getFavourite(u, v)) return 0;
  return kindIcon((ContactKind)v.type);
}

const char* DiscoverListModel::label(int i) const {
  static ContactView v;
  static char buf[44];
  if (!_svc || !_svc->getDiscovered(i, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}
uint16_t DiscoverListModel::icon(int i) const {
  static ContactView v;
  if (!_svc || !_svc->getDiscovered(i, v)) return 0;
  return kindIcon((ContactKind)v.type);
}

// raw autoadd_max_hops -> human label. 0=no limit, 1=direct (0 hops), N=up to N-1 hops.
static void maxHopsLabel(int raw, char* out, uint16_t cap) {
  if (raw <= 0)       snprintf(out, cap, "No limit");
  else if (raw == 1)  snprintf(out, cap, "Direct");
  else if (raw == 2)  snprintf(out, cap, "1 hop");
  else                snprintf(out, cap, "%d hops", raw - 1);
}

static const char* SETTINGS_LABELS[ContactsSettingsModel::ROW_COUNT] = {
  "Auto-add all", "Auto-add Users", "Auto-add Repeaters", "Auto-add Rooms", "Auto-add Sensors",
  "Overwrite oldest", "Max hops", "Remove non-users", "Remove non-favourites", "Remove all contacts",
};

bool ContactsSettingsModel::addAll() const {
  return _svc ? _svc->getAutoAdd().autoAddAll : true;
}

// Logical row sequence: the master toggle, then the per-kind toggles only when
// it's off, then overwrite + the cleanup actions.
ContactsSettingsModel::Row ContactsSettingsModel::rowAt(int i) const {
  Row seq[ROW_COUNT];
  int n = 0;
  seq[n++] = AutoAddAll;
  if (!addAll()) { seq[n++] = Users; seq[n++] = Repeaters; seq[n++] = Rooms; seq[n++] = Sensors; }
  seq[n++] = Overwrite;
  seq[n++] = MaxHops;
  seq[n++] = RemoveNonUsers; seq[n++] = RemoveNonFavourites; seq[n++] = RemoveAll;
  return (i >= 0 && i < n) ? seq[i] : ROW_COUNT;
}
int ContactsSettingsModel::count() const {
  // master + overwrite + maxhops + 3 actions, plus the 4 kind toggles when not adding all
  return addAll() ? 6 : ROW_COUNT;
}
const char* ContactsSettingsModel::label(int i) const {
  Row r = rowAt(i);
  return (r >= 0 && r < ROW_COUNT) ? SETTINGS_LABELS[r] : "";
}
bool ContactsSettingsModel::isToggle(int i) const {
  return rowAt(i) <= Overwrite;   // AutoAddAll..Overwrite are toggles; the rest are actions
}
bool ContactsSettingsModel::toggleState(int i) const {
  if (!_svc) return false;
  AutoAddConfig c = _svc->getAutoAdd();
  switch (rowAt(i)) {
    case AutoAddAll: return c.autoAddAll;
    case Users:      return c.addChat;
    case Repeaters:  return c.addRepeater;
    case Rooms:      return c.addRoom;
    case Sensors:    return c.addSensor;
    case Overwrite:  return c.overwriteOldest;
    default:         return false;
  }
}
const char* ContactsSettingsModel::value(int i) const {
  if (!_svc || rowAt(i) != MaxHops) return nullptr;   // only the MaxHops row shows a value
  static char buf[12];
  maxHopsLabel(_svc->getAutoAdd().maxHops, buf, sizeof(buf));
  return buf;
}

ContactsApplet::ContactsApplet()
    : Applet("Contacts"), _host(nullptr), _svc(nullptr),
      _confirming(false), _pendingAction(-1), _pickMode(false), _pickRequested(false),
      _editingHops(false) {}

static const char* emptyLabel(ContactKind k) {
  switch (k) {
    case ContactKind::Chat:     return "No contacts";
    case ContactKind::Repeater: return "No repeaters";
    case ContactKind::Room:     return "No rooms";
    default:                    return "No sensors";
  }
}

void ContactsApplet::syncListToTab() {
  const TabSlot& s = currentSlot();
  switch (s.kind) {
    case TabKind::Favourites: _list.setModel(&_favs); _list.setEmptyText("No favourites"); break;
    case TabKind::Discovered: _list.setModel(&_discover); _list.setEmptyText("No new devices"); break;
    case TabKind::Settings:   _list.setModel(&_settings); _list.setEmptyText(nullptr); break;
    default:                  _list.setModel(&_models[(int)s.contactKind - 1]);          // kinds are 1-based
                              _list.setEmptyText(emptyLabel(s.contactKind)); break;
  }
}

void ContactsApplet::rebuildTabs() {
  // Preserve what the user was viewing - the Favourites tab can appear or vanish
  // between visits, shifting indices.
  TabKind prevKind = TabKind::Kind; ContactKind prevContact = ContactKind::Chat;
  if (_slotCount > 0) { const TabSlot& s = currentSlot(); prevKind = s.kind; prevContact = s.contactKind; }

  _tabs.clear();   // resets selection to 0
  _slotCount = 0;
  int favCount = _pickMode ? _favs.count() : (_svc ? _svc->countFavourites() : 0);
  if (favCount > 0) {
    _tabs.addTab("Favorites", (uint16_t)Icon::Star);
    _slots[_slotCount++] = {TabKind::Favourites, ContactKind::Chat};
  }
  if (_pickMode) {
    _tabs.addTab("Contacts", tabIcon(ContactKind::Chat));
    _slots[_slotCount++] = {TabKind::Kind, ContactKind::Chat};
  } else {
    static const ContactKind KINDS[4] = {
      ContactKind::Chat, ContactKind::Repeater, ContactKind::Room, ContactKind::Sensor };
    static const char* KIND_LABELS[4] = { "Contacts", "Repeaters", "Rooms", "Sensors" };
    for (int i = 0; i < 4; i++) {
      _tabs.addTab(KIND_LABELS[i], tabIcon(KINDS[i]));
      _slots[_slotCount++] = {TabKind::Kind, KINDS[i]};
    }
    _tabs.addTab("Discover", (uint16_t)Icon::Search);   // seen-but-not-added nodes
    _slots[_slotCount++] = {TabKind::Discovered, ContactKind::Chat};
    _tabs.addTab("Settings", (uint16_t)Icon::Settings);
    _slots[_slotCount++] = {TabKind::Settings, ContactKind::Chat};
  }

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
  _app = ctx.app;
  _pickMode = _pickRequested; _pickRequested = false;
  _models[0].bind(_svc, ContactKind::Chat, kindIcon(ContactKind::Chat));
  _models[1].bind(_svc, ContactKind::Repeater, kindIcon(ContactKind::Repeater));
  _models[2].bind(_svc, ContactKind::Room, kindIcon(ContactKind::Room));
  _models[3].bind(_svc, ContactKind::Sensor, kindIcon(ContactKind::Sensor));
  _favs.bind(_svc);
  _favs.setUsersOnly(_pickMode);
  _discover.bind(_svc);
  _settings.bind(_svc);
  _list.setRowHeight(14);
  _confirming = false; _pendingAction = -1;
  _editingHops = false;
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
  if (_editingHops) { _hops.draw(c, 0, 0, w, h); return 100; }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool ContactsApplet::onInput(InputEvent ev) {
  if (_editingHops) {
    if (_hops.onInput(ev)) {
      StepperResult r = _hops.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _svc) {
          AutoAddConfig cfg = _svc->getAutoAdd();
          cfg.maxHops = (uint8_t)_hops.value();
          _svc->setAutoAdd(cfg);
        }
        _editingHops = false;
        _hops.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _svc) {
          int removed = -1;
          if (_pendingAction == ContactsSettingsModel::RemoveNonUsers) removed = _svc->removeNonChat();
          else if (_pendingAction == ContactsSettingsModel::RemoveNonFavourites) removed = _svc->removeNonFavourites();
          else if (_pendingAction == ContactsSettingsModel::RemoveAll) removed = _svc->removeAll();
          if (removed >= 0 && _host) {
            char buf[20]; snprintf(buf, sizeof(buf), "Removed %d", removed);
            _host->postToast(buf);
          }
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
      ContactsSettingsModel::Row r = _settings.rowAt(i);
      if (_settings.isToggle(i) && _svc) {
        AutoAddConfig cfg = _svc->getAutoAdd();
        switch (r) {
          case ContactsSettingsModel::AutoAddAll: cfg.autoAddAll = !cfg.autoAddAll; break;
          case ContactsSettingsModel::Users:      cfg.addChat = !cfg.addChat; break;
          case ContactsSettingsModel::Repeaters:  cfg.addRepeater = !cfg.addRepeater; break;
          case ContactsSettingsModel::Rooms:      cfg.addRoom = !cfg.addRoom; break;
          case ContactsSettingsModel::Sensors:    cfg.addSensor = !cfg.addSensor; break;
          case ContactsSettingsModel::Overwrite:  cfg.overwriteOldest = !cfg.overwriteOldest; break;
          default: break;
        }
        _svc->setAutoAdd(cfg);
      } else if (r == ContactsSettingsModel::MaxHops && _svc) {
        _hops.configure("Max hops", _svc->getAutoAdd().maxHops, 0, 64, maxHopsLabel);
        _editingHops = true;
      } else {
        _pendingAction = r;   // store the logical Row, not the (dynamic) list index
        const char* msg = "Remove non-user contacts?";
        if (r == ContactsSettingsModel::RemoveAll) msg = "Remove ALL contacts?";
        else if (r == ContactsSettingsModel::RemoveNonFavourites) msg = "Remove all non-favourites?";
        _confirm.configure(msg);
        _confirming = true;
      }
      return true;
    }
    if (_svc) {
      const TabSlot& s = currentSlot();
      ContactView v;
      if (s.kind == TabKind::Discovered) {                 // open the discovery detail (add from there)
        if (_svc->getDiscovered(_list.selected(), v)) {
          discoverDetailApplet().setTarget(v);
          if (_host) _host->push(&discoverDetailApplet());
        }
        return true;
      }
      // Favourites or a kind tab: drill into detail (or open thread in pick mode).
      // For Favourites, resolve filtered index -> raw service index via rawIndex() so that
      // users-only filtering (pick mode) doesn't silently fetch the wrong entry.
      bool ok;
      if (s.kind == TabKind::Favourites) {
        int raw = _favs.rawIndex(_list.selected());
        ok = (raw >= 0) && _svc->getFavourite(raw, v);
      } else {
        ok = _svc->getByKind(s.contactKind, _list.selected(), v);
      }
      if (ok) {
        if (_pickMode) {
          messageThreadApplet().setTarget(directKey(v.pubKey), v.name);
          messageThreadApplet().composeOnOpen();   // picker = intent to write -> focus Write button
          if (_host) _host->replace(&messageThreadApplet());
        } else {
          contactDetailApplet().setTarget(v.pubKey);
          if (_host) _host->push(&contactDetailApplet());
        }
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

MISHMESH_REGISTER_APPLET_ICON(&contactsApplet(), Placement::AppMenu, "Contacts", 1,
                              (uint16_t)Icon::Users);

}  // namespace mishmesh
