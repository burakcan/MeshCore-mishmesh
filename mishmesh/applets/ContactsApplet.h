#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ConfirmDialog.h>

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
public:
  FavouritesListModel() : _svc(nullptr) {}
  void bind(ContactsService* svc) { _svc = svc; }
  int count() const override { return _svc ? _svc->countFavourites() : 0; }
  const char* label(int i) const override;
  uint16_t icon(int i) const override;
};

// Fixed settings rows (4 auto-add toggles + overwrite + 2 cleanup actions).
class ContactsSettingsModel : public ListModel {
  ContactsService* _svc;
public:
  enum Row { Users, Repeaters, Rooms, Sensors, Overwrite, RemoveNonUsers, RemoveAll, ROW_COUNT };
  ContactsSettingsModel() : _svc(nullptr) {}
  void bind(ContactsService* svc) { _svc = svc; }
  int count() const override { return ROW_COUNT; }
  const char* label(int i) const override;
  bool isToggle(int i) const override { return i <= Overwrite; }
  bool toggleState(int i) const override;
};

class ContactsApplet : public Applet {
  // What a given tab shows. The Favourites tab exists only when there are
  // favourites, so tab indices shift; a slot per tab records its meaning.
  enum class TabKind : uint8_t { Favourites, Kind, Settings };
  struct TabSlot { TabKind kind; ContactKind contactKind; };

  AppletHost*           _host;
  ContactsService*      _svc;
  TabBar                _tabs;
  ListMenu              _list;
  ContactListModel      _models[4];     // Chat/Repeater/Room/Sensor
  FavouritesListModel   _favs;
  ContactsSettingsModel _settings;
  ConfirmDialog         _confirm;
  TabSlot               _slots[6];      // parallel to the tabs
  int                   _slotCount;
  bool                  _confirming;
  int                   _pendingAction; // ContactsSettingsModel::Row for cleanup confirm

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
};

ContactsApplet& contactsApplet();   // shared static instance

}  // namespace mishmesh
