// mishmesh/applets/ServerLoginApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

// Logs into a server-type contact (Room or Repeater) with a password, then hands
// off to the mode's destination: a Room opens its message thread; a Repeater opens
// its management screen. Opens straight through when already logged in this power
// cycle; else auto-logs in with a password remembered in AppletStorage, or prompts
// via the keypad. Polls ContactsService::loginSeq() for the async server result
// and, on a freshly-typed password, offers to remember it. Host-safe.
class ServerLoginApplet : public Applet {
public:
  enum class Mode : uint8_t { Room, Repeater };

  ServerLoginApplet() : Applet("Server login") {}
  void setTarget(const uint8_t* pubKey, const char* name, Mode mode);
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

private:
  enum class Phase : uint8_t { Start, Entering, WaitingLogin, AskRemember };

  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppletContext*   _ctx = nullptr;
  AppletStorage*   _storage = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  char     _pwBuf[KeypadApplet::KP_MAX + 1] = {0};
  Mode     _mode = Mode::Room;
  Phase    _phase = Phase::Start;
  bool     _submitted = false;
  bool     _freshPw = false;
  uint32_t _seqStart = 0;
  uint32_t _waitStartMs = 0;
  ConfirmDialog _remember;

  void startLogin();
  void promptPassword();
  void onSuccess();              // route to the mode's destination applet
  int  storageKey(char* dst, int cap) const;   // "rmpw"/"repw" + hex6
  static void onPwDone(void* ctx, const char* text);

  static const uint32_t LOGIN_TIMEOUT_MS = 25000;
};

ServerLoginApplet& serverLoginApplet();

}  // namespace mishmesh
