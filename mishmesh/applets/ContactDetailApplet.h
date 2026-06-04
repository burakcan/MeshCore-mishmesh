#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/PingDialog.h>
#include <mishmesh/widgets/TelemetryDialog.h>
#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/widgets/Card.h>
#include <mishmesh/applets/KeypadApplet.h>

namespace mishmesh {

class ContactDetailApplet : public Applet, public ListModel {
public:
  enum Action { View, Favourite, Telemetry, Ping, ResetPath, ClearConvo, Delete, Message, Rename, ACTION_KINDS };
private:
  AppletHost*      _host;
  ContactsService* _svc;
  AppServices*     _app;
  uint8_t          _pubkey[6];
  uint8_t          _fullKey[PUBKEY_LEN];
  char             _name[32];
  char             _displayName[44];         // name, with a key prefix for repeaters
  char             _infoLine[48];            // "Type · distance · last heard"
  uint8_t          _type;
  bool             _hasPath;
  uint8_t          _hops;
  bool             _favourite;
  uint32_t         _lastAdvert;
  bool             _hasLoc;
  int32_t          _gpsLat, _gpsLon;
  float            _distKm;                  // -1 if unknown
  uint8_t          _actions[ACTION_KINDS];
  int              _actionCount;
  ListMenu         _list;
  ConfirmDialog    _confirm;
  PingDialog       _ping;
  TelemetryDialog  _telem;
  Card             _card;                    // header card (name + info)
  ScrollText       _details;                 // the view-details panel
  bool             _confirming;
  bool             _viewing;
  bool             _pinging;
  bool             _pingDone;
  uint32_t         _pingStartSeq;
  uint32_t         _pingStartMs;
  bool             _telemActive;
  bool             _telemDone;
  uint32_t         _telemStartSeq;
  uint32_t         _telemStartMs;
  int              _pendingAction;

  char             _renameBuf[32];   // backs the keypad while renaming (ContactInfo.name is [32])

  void refresh();        // pull current contact into the detail fields + cards
  void buildActions();   // fill _actions from _type
  void buildInfo();      // build _displayName / _infoLine / _details from the fields
  static void onRenameDone(void* ctx, const char* text);   // keypad confirm -> rename + refresh
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
