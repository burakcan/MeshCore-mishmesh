#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/applets/settings/ContactsSettingsPanel.h>

namespace mishmesh {

// Upper bound on contacts the list cache indexes; matches the largest board's
// MAX_CONTACTS (Wio Tracker L1 = 350). Contacts beyond this simply aren't shown
// by a cached tab - a soft ceiling, not a crash.
#ifndef MISHMESH_CONTACT_CACHE_MAX
#define MISHMESH_CONTACT_CACHE_MAX 350
#endif

// Caches "display row -> raw contact index" for one filter (a contact kind, or the
// favourites set), rebuilt only when ContactsService::contactsSeq() changes. This
// turns list navigation from an O(contacts) table scan per row per frame into an
// O(1) index lookup. Only one tab renders at a time, so a single instance,
// reconfigured on tab switch via configure(), serves every list model.
class ContactFilterCache {
public:
  enum Filter : uint8_t { Kind, Favourites, FavouriteUsers };
  void bind(ContactsService* svc) { _svc = svc; _valid = false; }
  // Point the cache at a filter. Cheap and idempotent: only invalidates on change.
  void configure(Filter f, ContactKind kind) {
    if (f != _filter || kind != _kind) { _filter = f; _kind = kind; _valid = false; }
  }
  int  count() const { ensure(); return _count; }
  bool at(int display, ContactView& out) const {
    ensure();
    if (!_svc || display < 0 || display >= _count) return false;
    return _svc->contactAt(_idx[display], out);
  }
  // Raw contact index behind a display row (-1 if none); used to open a detail.
  int  rawIndex(int display) const { ensure(); return (display >= 0 && display < _count) ? _idx[display] : -1; }

private:
  void ensure() const;   // rebuild _idx from the service when contactsSeq() moved

  ContactsService* _svc = nullptr;
  Filter           _filter = Kind;
  ContactKind      _kind = ContactKind::Chat;
  mutable uint16_t _idx[MISHMESH_CONTACT_CACHE_MAX];
  mutable int      _count = 0;
  mutable uint32_t _builtSeq = 0;
  mutable bool     _valid = false;
};

// ListModel over one contact kind, resolved through the shared filter cache.
class ContactListModel : public ListModel {
  ContactFilterCache* _cache;
  ContactKind         _kind;
  uint16_t            _icon;
  void sync() const { if (_cache) _cache->configure(ContactFilterCache::Kind, _kind); }
public:
  ContactListModel() : _cache(nullptr), _kind(ContactKind::Chat), _icon(0) {}
  void bind(ContactFilterCache* cache, ContactKind k, uint16_t icon) { _cache = cache; _kind = k; _icon = icon; }
  ContactKind kind() const { return _kind; }
  int count() const override { sync(); return _cache ? _cache->count() : 0; }
  const char* label(int i) const override;
  uint16_t icon(int) const override { return _icon; }
};

// Favourites span all kinds; each row carries its own contact-type icon.
class FavouritesListModel : public ListModel {
  ContactFilterCache* _cache;
  bool                _usersOnly;
  void sync() const {
    if (_cache) _cache->configure(_usersOnly ? ContactFilterCache::FavouriteUsers
                                             : ContactFilterCache::Favourites, ContactKind::Chat);
  }
public:
  FavouritesListModel() : _cache(nullptr), _usersOnly(false) {}
  void bind(ContactFilterCache* cache) { _cache = cache; }
  void setUsersOnly(bool u) { _usersOnly = u; }
  int rawIndex(int filtered) const { sync(); return _cache ? _cache->rawIndex(filtered) : -1; }
  int count() const override { sync(); return _cache ? _cache->count() : 0; }
  const char* label(int i) const override;
  uint16_t icon(int i) const override;
};

// Discovered (seen-but-not-added) nodes, with a per-row contact-type icon.
class DiscoverListModel : public ListModel {
  ContactsService* _svc;
public:
  DiscoverListModel() : _svc(nullptr) {}
  void bind(ContactsService* svc) { _svc = svc; }
  int count() const override { return _svc ? _svc->countDiscovered() : 0; }
  const char* label(int i) const override;
  uint16_t icon(int i) const override;
};

class ContactsApplet : public Applet {
  // What a given tab shows. The Favourites tab exists only when there are
  // favourites, so tab indices shift; a slot per tab records its meaning.
  enum class TabKind : uint8_t { Favourites, Kind, Discovered, Settings };
  struct TabSlot { TabKind kind; ContactKind contactKind; };

  AppletHost*           _host;
  ContactsService*      _svc;
  AppServices*          _app = nullptr;   // for the tab-bar battery decoration
  char                  _battBuf[8] = {0};
  TabBar                _tabs;
  ListMenu              _list;
  ContactFilterCache    _cache;         // shared row-index cache for the active tab
  ContactListModel      _models[4];     // Chat/Repeater/Room/Sensor
  FavouritesListModel   _favs;
  DiscoverListModel     _discover;
  TabSlot               _slots[8];      // parallel to the tabs
  int                   _slotCount;
  bool                  _pickMode;        // picker: only Favourites(users) + Contacts(users), select -> thread
  bool                  _pickRequested;   // one-shot, consumed by onStart

  const TabSlot& currentSlot() const { return _slots[_tabs.selected()]; }
  bool settingsTab() const { return currentSlot().kind == TabKind::Settings; }
  void rebuildTabs();    // (re)build the tab set; the Favourites tab is conditional
  void syncListToTab();

public:
  ContactsApplet();
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  void beginPick() { _pickRequested = true; }
  int  tabCountForTest() const { return _slotCount; }
  bool pickModeForTest() const { return _pickMode; }
};

ContactsApplet& contactsApplet();   // shared static instance

}  // namespace mishmesh
