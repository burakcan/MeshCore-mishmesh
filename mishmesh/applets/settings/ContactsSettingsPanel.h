#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

// Settings rows. The per-kind auto-add toggles only appear when "Auto-add all"
// is off (mirrors the companion app), so the visible row set is dynamic - map
// list index -> logical Row via rowAt(). Kept top-level for existing tests.
class ContactsSettingsModel : public ListModel {
  ContactsService* _svc;
  bool addAll() const;   // current master state (defaults to true when unbound)
public:
  enum Row { AutoAddAll, Users, Repeaters, Rooms, Sensors, Overwrite, NotifyFull, MaxHops,
             RemoveNonUsers, RemoveNonFavourites, RemoveAll, ROW_COUNT };
  ContactsSettingsModel() : _svc(nullptr) {}
  void bind(ContactsService* svc) { _svc = svc; }
  int  buildRows(Row* seq) const;   // fills the visible-row sequence; returns count
  Row rowAt(int i) const;
  int count() const override;
  const char* label(int i) const override;
  bool isToggle(int i) const override;
  bool toggleState(int i) const override;
  const char* value(int i) const override;
};

// Auto-add config (toggles + max-hops stepper) and contact-cleanup actions
// (remove-confirm). Source of truth: ContactsService.
class ContactsSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Contacts"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editingHops || _confirming; }

private:
  ContactsSettingsModel _model;
  ContactsService*      _svc  = nullptr;
  AppletHost*           _host = nullptr;
  ListMenu              _list;
  StepperDialog         _hops;
  ConfirmDialog         _confirm;
  bool                  _editingHops = false;
  bool                  _confirming  = false;
  int                   _pendingAction = -1;   // ContactsSettingsModel::Row for cleanup confirm
};

ContactsSettingsPanel& contactsSettings();   // shared singleton

}  // namespace mishmesh
