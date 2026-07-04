// mishmesh/applets/RepeaterIdentityApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/StatusBar.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

// Change a logged-in repeater's Ed25519 identity key. Two modes: random keygen or
// a 64-hex user-supplied seed. Shows the resulting 128-hex private key, asks a
// destructive confirm, then applies via "set prv.key <hex>" + "reboot". Launch-only.
// Host-safe (keygen is behind the ContactsService::makeIdentityHex seam).
class RepeaterIdentityApplet : public Applet {
public:
  RepeaterIdentityApplet() : Applet("Identity") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  enum class Phase : uint8_t { Menu, Enter, Show, Confirm, Busy, Done };
  Phase       phaseForTest()    const { return _phase; }
  const char* shownKeyForTest() const { return _keyHex; }

  // Public so host tests can simulate the keypad confirm callback directly.
  static void onSeedConfirm(void* ctx, const char* text);

private:
  AppletHost*      _host = nullptr;
  ContactsService* _svc  = nullptr;
  AppServices*     _app  = nullptr;
  uint8_t _pub[6]   = {0};
  char    _name[32] = {0};

  Phase    _phase     = Phase::Menu;
  bool     _seedReady = false;

  char _keyHex[129] = {0};    // 128-hex private key + NUL
  char _kpBuf[65]   = {0};    // keypad destination buffer + NUL
  char _seedBuf[65] = {0};    // seed copied from keypad on confirm + NUL
  char _cmd[141]    = {0};    // "set prv.key " (12) + 128 hex + NUL = 141
  char _status[80]  = {0};    // pubkey or error shown in Done/Menu

  uint32_t       _startSeq = 0;
  PendingRequest _req;

  StatusBar     _bar;
  ConfirmDialog _confirm;
  ScrollText    _scroll;

  StaticListModel _menuModel;
  ListMenu _menu;

  void doGenerate();
  void doSetKey();
  void buildScrollText();
};

RepeaterIdentityApplet& repeaterIdentityApplet();

}  // namespace mishmesh
