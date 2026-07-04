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

// Rebuild the display-row -> raw-contact-index map from the service, but only when
// the contact table actually changed (contactsSeq). One O(contacts) pass here
// replaces an O(contacts) scan on every row of every frame.
void ContactFilterCache::ensure() const {
  if (!_svc) { _count = 0; return; }
  uint32_t seq = _svc->contactsSeq();
  if (_valid && seq == _builtSeq) return;
  _builtSeq = seq;
  _count = 0;
  int n = _svc->contactCount();
  ContactView v;
  for (int i = 0; i < n && _count < MISHMESH_CONTACT_CACHE_MAX; i++) {
    if (!_svc->contactAt(i, v)) continue;
    bool match;
    switch (_filter) {
      case Favourites:     match = v.isFavourite; break;
      case FavouriteUsers: match = v.isFavourite && v.type == (uint8_t)ContactKind::Chat; break;
      default:             match = v.type == (uint8_t)_kind; break;
    }
    if (match) _idx[_count++] = (uint16_t)i;
  }
  _valid = true;
}

const char* ContactListModel::label(int i) const {
  static ContactView v;
  static char buf[44];
  sync();
  if (!_cache || !_cache->at(i, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}

const char* FavouritesListModel::label(int i) const {
  static ContactView v;
  static char buf[44];
  sync();
  if (!_cache || !_cache->at(i, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}
uint16_t FavouritesListModel::icon(int i) const {
  static ContactView v;
  sync();
  if (!_cache || !_cache->at(i, v)) return 0;
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


ContactsApplet::ContactsApplet()
    : Applet("Contacts"), _host(nullptr), _svc(nullptr),
      _pickMode(false), _pickRequested(false) {}

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
    case TabKind::Settings:   _list.setEmptyText(nullptr); break;   // rendered by contactsSettings()
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
  if (!_pickMode) { _pickFn = nullptr; }
  _cache.bind(_svc);
  _models[0].bind(&_cache, ContactKind::Chat, kindIcon(ContactKind::Chat));
  _models[1].bind(&_cache, ContactKind::Repeater, kindIcon(ContactKind::Repeater));
  _models[2].bind(&_cache, ContactKind::Room, kindIcon(ContactKind::Room));
  _models[3].bind(&_cache, ContactKind::Sensor, kindIcon(ContactKind::Sensor));
  _favs.bind(&_cache);
  _favs.setUsersOnly(_pickMode);
  _discover.bind(_svc);
  contactsSettings().begin(ctx);
  _list.setRowHeight(14);
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
  if (settingsTab()) return contactsSettings().renderBody(c, 0, bodyY, w, bodyH);
  _list.draw(c, 0, bodyY, w, bodyH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool ContactsApplet::onInput(InputEvent ev) {
  if (settingsTab()) {
    if (contactsSettings().modalActive()) return contactsSettings().onInput(ev);
    if (_tabs.onInput(ev)) { syncListToTab(); return true; }   // NavLeft/Right leave settings
    if (contactsSettings().onInput(ev)) return true;
    return false;   // Back bubbles to the host
  }

  if (_tabs.onInput(ev)) { syncListToTab(); return true; }
  if (_list.onInput(ev)) return true;

  if (ev == InputEvent::Select) {
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
      // For Favourites, resolve the display row -> raw contact index via rawIndex()
      // so users-only filtering (pick mode) doesn't silently fetch the wrong entry.
      bool ok;
      if (s.kind == TabKind::Favourites) {
        int raw = _favs.rawIndex(_list.selected());
        ok = (raw >= 0) && _svc->contactAt(raw, v);
      } else {
        ok = _svc->getByKind(s.contactKind, _list.selected(), v);
      }
      if (ok) {
        if (_pickMode) {
          if (_pickFn) {                       // caller wants the pubkey back
            ContactPickFn fn = _pickFn; void* cx = _pickCtx;
            _pickFn = nullptr; _pickCtx = nullptr;
            fn(cx, v.pubKey);                  // set the caller's state FIRST
            if (_host) _host->pop();           // then return to the caller (fires onForeground)
            return true;
          }
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
