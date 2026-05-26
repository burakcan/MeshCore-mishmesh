#pragma once

#include <string.h>

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>

namespace mishmesh {

// Fires a ping (status request) at a contact and shows the round-trip result.
class PingApplet : public Applet {
  AppletHost*      _host;
  ContactsService* _svc;
  uint8_t          _pubkey[6];
  uint32_t         _startSeq;
  uint32_t         _startMs;
  bool             _done;
  PingView         _result;
public:
  PingApplet();
  void setTarget(const uint8_t* pubKey) { memcpy(_pubkey, pubKey, 6); }
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

PingApplet& pingApplet();
void pingSetTarget(const uint8_t* pubKey);

}  // namespace mishmesh
