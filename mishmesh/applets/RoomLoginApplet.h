// mishmesh/applets/RoomLoginApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

// Logs into a Room-server contact, then hands off to its message thread. Opens
// straight through when the room is already logged in this power cycle; else
// auto-logs in with a password remembered in AppletStorage, or prompts via the
// keypad. Polls ContactsService::loginSeq() for the async server result and, on a
// freshly-typed password, offers to remember it. Host-safe (no Arduino deps).
class RoomLoginApplet : public Applet {
public:
  RoomLoginApplet() : Applet("Room login") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

private:
  // Start: first render decides the path. Entering: keypad is up. WaitingLogin:
  // awaiting the server round-trip. AskRemember: offering to save the password.
  enum class Phase : uint8_t { Start, Entering, WaitingLogin, AskRemember };

  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppletContext*   _ctx = nullptr;
  AppletStorage*   _storage = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  char     _pwBuf[KeypadApplet::KP_MAX + 1] = {0};
  Phase    _phase = Phase::Start;
  bool     _submitted = false;   // keypad OK'd (vs. cancelled -> abort)
  bool     _freshPw = false;     // typed this session -> offer to remember on success
  uint32_t _seqStart = 0;        // loginSeq() at send time
  uint32_t _waitStartMs = 0;     // c.now() when the wait began (0 = unstamped)
  ConfirmDialog _remember;

  void startLogin();             // send password, enter WaitingLogin
  void promptPassword();         // push the keypad for entry
  void openThread();             // replace self with the room's thread
  int  storageKey(char* dst, int cap) const;   // "rmpw<hex6>"
  static void onPwDone(void* ctx, const char* text);

  static const uint32_t LOGIN_TIMEOUT_MS = 25000;   // server may flood both ways
};

RoomLoginApplet& roomLoginApplet();

}  // namespace mishmesh
