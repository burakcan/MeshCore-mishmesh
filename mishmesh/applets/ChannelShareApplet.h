// mishmesh/applets/ChannelShareApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/QrView.h>

namespace mishmesh {

// Shares a channel/group. Reached from ChatMenu's "Share" row (channels only).
// Shows a QR of a self-describing join URI:
//   meshcore:channel/add?name=<url-encoded name>&secret=<32-hex PSK>
// Select toggles between the QR and the raw hex key (so it can be read/typed by
// hand). Back pops. Launch-only singleton - no menu registration.
class ChannelShareApplet : public Applet {
public:
  ChannelShareApplet() : Applet("Share channel") {}
  void setTarget(const ConvoKey& key, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Test hooks.
  const char* uriForTest() const { return _uri; }
  const char* keyHexForTest() const { return _keyHex; }
  bool showingKeyForTest() const { return _showKey; }

private:
  void buildUri();

  MessagesService* _svc = nullptr;
  ConvoKey         _key{};
  bool             _showKey = false;
  char             _name[32] = {0};
  char             _keyHex[33] = {0};   // 32 hex chars + NUL
  char             _uri[200] = {0};     // prefix + %-encoded name + &secret= + 32 hex
  QrView           _qr;
};

ChannelShareApplet& channelShareApplet();

}  // namespace mishmesh
