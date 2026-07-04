// mishmesh/applets/CommandLineApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

// A raw CLI console for a logged-in repeater/room: Select opens the keypad to type
// a command, which is sent over the ContactsService CLI channel; the async reply is
// appended to a scrollable, oldest-drop RAM log (this session only, not persisted).
// Reached from the repeater management hub, so it is launch-only (not registered).
// Host-safe (no Arduino deps).
class CommandLineApplet : public Applet {
public:
  CommandLineApplet() : Applet("Command line") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Embedded-view protocol (called by RepeaterManageApplet as tab host).
  void onShow(AppletContext& ctx);
  // noinline: onRender() just delegates here; without it the body is also inlined into
  // the vtable-referenced onRender(), duplicating ~640B of flash (the BLE build is tight).
  int  renderBody(Canvas& c, int x, int y, int w, int h) __attribute__((noinline));

  // Send a command now: echo "> cmd", fire the request, start awaiting the reply.
  // Public because the keypad confirm callback and host tests both drive it.
  void submitCommand(const char* cmd);

  const uint8_t* targetForTest() const { return _pub; }
  int  logCountForTest() const { return _logCount; }
  const char* logLineForTest(int i) const { return (i >= 0 && i < _logCount) ? _log[i] : ""; }

private:
  static const int LOG_LINES = 16;
  static const int LOG_COLS  = ScrollText::LINE_LEN;   // keep the log line width == the view's, no phantom storage

  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  char     _cmdBuf[KeypadApplet::KP_MAX + 1] = {0};
  char     _log[LOG_LINES][LOG_COLS];
  int      _logCount = 0;
  bool     _pending = false;
  uint32_t _cliStartSeq = 0;
  PendingRequest _req;
  ScrollText     _view;

  void pushLine(const char* s);          // append one line, oldest-drop when full
  void appendMultiline(const char* s);   // split on '\n', pushLine each segment
  void syncView();                       // rebuild _view from _log, snap to bottom
  static void onCmdDone(void* ctx, const char* text);
};

CommandLineApplet& commandLineApplet();

}  // namespace mishmesh
