// mishmesh/applets/RepeaterActionsPanel.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// One-shot repeater commands (not staged): Send advert, Sync clock, Reboot. Each fires a
// single CLI command; Reboot asks a confirm first (the session drops, no reply). Advert and
// Sync await the repeater's CLI reply before toasting the result. Host-safe.
class RepeaterActionsPanel : public Applet {
public:
  enum class Phase : uint8_t { Idle, Busy };

  RepeaterActionsPanel() : Applet("Actions") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  Phase phaseForTest() const { return _phase; }

private:
  enum { ROW_ADVERT = 0, ROW_SYNC = 1, ROW_REBOOT = 2, ROW_COUNT = 3 };
  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppServices*     _app = nullptr;
  uint8_t _pub[6] = {0};
  char    _name[32] = {0};
  bool    _confirming = false;
  Phase   _phase = Phase::Idle;
  uint32_t _startSeq = 0;
  PendingRequest _req;
  StatusBar _bar;
  ListMenu _menu;
  ConfirmDialog _confirm;

  void fireAdvert();
  void fireSync();
  void fireReboot();
};

RepeaterActionsPanel& repeaterActionsPanel();

}  // namespace mishmesh
