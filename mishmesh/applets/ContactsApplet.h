#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/applets/settings/ContactsSettingsPanel.h>

namespace mishmesh {

// ListModel over one contact kind, read from the service on demand.
class ContactListModel : public ListModel {
  ContactsService* _svc;
  ContactKind      _kind;
  uint16_t         _icon;
public:
  ContactListModel() : _svc(nullptr), _kind(ContactKind::Chat), _icon(0) {}
  void bind(ContactsService* svc, ContactKind k, uint16_t icon) { _svc = svc; _kind = k; _icon = icon; }
  ContactKind kind() const { return _kind; }
  int count() const override { return _svc ? _svc->countByKind(_kind) : 0; }
  const char* label(int i) const override;
  uint16_t icon(int) const override { return _icon; }
};

// Favourites span all kinds; each row carries its own contact-type icon.
class FavouritesListModel : public ListModel {
  ContactsService* _svc;
  bool             _usersOnly;
  int  underlying(int filtered) const;   // map filtered idx -> raw favourite idx (-1 if none)
public:
  FavouritesListModel() : _svc(nullptr), _usersOnly(false) {}
  void bind(ContactsService* svc) { _svc = svc; }
  void setUsersOnly(bool u) { _usersOnly = u; }
  int rawIndex(int filtered) const { return underlying(filtered); }
  int count() const override;
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
