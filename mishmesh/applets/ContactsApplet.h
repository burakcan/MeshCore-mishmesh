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
  AppletHost*           _host;
  ContactsService*      _svc;
  TabBar                _tabs;
  ListMenu              _list;
  ContactListModel      _models[4];     // Chat/Repeater/Room/Sensor
  ContactsSettingsModel _settings;
  ConfirmDialog         _confirm;
  bool                  _confirming;
  int                   _pendingAction; // ContactsSettingsModel::Row for cleanup confirm

  static const int SETTINGS_TAB = 4;
  bool settingsTab() const { return _tabs.selected() == SETTINGS_TAB; }
  ContactKind currentKind() const;
  void syncListToTab();

public:
  ContactsApplet();
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

ContactsApplet& contactsApplet();   // shared static instance

}  // namespace mishmesh
