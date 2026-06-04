#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/StepperDialog.h>

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

// Settings rows. The per-kind auto-add toggles only appear when "Auto-add all"
// is off (mirrors the companion app's all/selected modes), so the visible row
// set is dynamic — map list index -> logical Row via rowAt().
class ContactsSettingsModel : public ListModel {
  ContactsService* _svc;
  bool addAll() const;   // current master state (defaults to true when unbound)
public:
  enum Row { AutoAddAll, Users, Repeaters, Rooms, Sensors, Overwrite, MaxHops,
             RemoveNonUsers, RemoveNonFavourites, RemoveAll, ROW_COUNT };
  ContactsSettingsModel() : _svc(nullptr) {}
  void bind(ContactsService* svc) { _svc = svc; }
  Row rowAt(int i) const;   // visible list index -> logical Row (ROW_COUNT if out of range)
  int count() const override;
  const char* label(int i) const override;
  bool isToggle(int i) const override;
  bool toggleState(int i) const override;
  const char* value(int i) const override;
};

class ContactsApplet : public Applet {
  // What a given tab shows. The Favourites tab exists only when there are
  // favourites, so tab indices shift; a slot per tab records its meaning.
  enum class TabKind : uint8_t { Favourites, Kind, Discovered, Settings };
  struct TabSlot { TabKind kind; ContactKind contactKind; };

  AppletHost*           _host;
  ContactsService*      _svc;
  TabBar                _tabs;
  ListMenu              _list;
  ContactListModel      _models[4];     // Chat/Repeater/Room/Sensor
  FavouritesListModel   _favs;
  DiscoverListModel     _discover;
  ContactsSettingsModel _settings;
  ConfirmDialog         _confirm;
  StepperDialog         _hops;          // max-hops value picker (modal)
  bool                  _editingHops;
  TabSlot               _slots[8];      // parallel to the tabs
  int                   _slotCount;
  bool                  _confirming;
  int                   _pendingAction; // ContactsSettingsModel::Row for cleanup confirm
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
