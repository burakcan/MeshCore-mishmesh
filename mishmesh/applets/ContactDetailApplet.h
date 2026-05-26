#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Label.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/Toast.h>

namespace mishmesh {

class ContactDetailApplet : public Applet, public ListModel {
public:
  enum Action { View, Telemetry, Ping, ResetPath, ClearConvo, Delete, ACTION_KINDS };
private:
  AppletHost*      _host;
  ContactsService* _svc;
  AppServices*     _app;
  uint8_t          _pubkey[6];
  char             _name[32];
  uint8_t          _type;
  bool             _hasPath;
  bool             _favourite;
  uint32_t         _lastAdvert;
  uint8_t          _actions[ACTION_KINDS];   // action ids present for this contact type
  int              _actionCount;
  Label            _header;
  ListMenu         _list;
  ConfirmDialog    _confirm;
  Toast            _toast;
  const char*      _pendingToast;            // latched in onInput, shown in onRender
  bool             _confirming;
  bool             _viewing;                 // showing the details panel
  int              _pendingAction;           // action id awaiting confirm

  void refresh();        // pull current contact into the detail fields
  void buildActions();   // fill _actions from _type
  void drawDetails(Canvas& c, int x, int y, int w, int h);
public:
  ContactDetailApplet();
  void setTarget(const uint8_t* pubKey);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  // ListModel (the action list)
  int count() const override { return _actionCount; }
  const char* label(int i) const override;
};

ContactDetailApplet& contactDetailApplet();

}  // namespace mishmesh
