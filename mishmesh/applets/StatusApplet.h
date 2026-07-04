// mishmesh/applets/StatusApplet.h
#pragma once
#include <stdint.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/widgets/Button.h>

namespace mishmesh {

static const int STATUS_LINE_LEN  = 40;
static const int STATUS_MAX_LINES = 16;

// Pure formatter: one display line per metric group into `out`, returning the count
// (<= maxLines). An invalid view renders a single "No data" line; a 0 login clock
// renders "Clock: --". Host-testable, no Arduino deps.
int formatRepeaterStatus(const RepeaterStatusView& s, uint32_t loginClockEpoch,
                         char out[][STATUS_LINE_LEN], int maxLines);

// Read-on-demand repeater status: fetches RepeaterStats on open (reusing PendingRequest),
// renders it via formatRepeaterStatus into a scrollable ScrollText. Select re-fetches;
// no auto-poll (each fetch is a LoRa round-trip). Launch-only (pushed from the hub).
class StatusApplet : public Applet {
public:
  StatusApplet() : Applet("Status") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Embedded-view protocol (called by RepeaterManageApplet as tab host).
  // onShow fires the status request; renderBody draws into the given sub-region.
  void onShow(AppletContext& ctx);
  // noinline: onRender() delegates here; without it the body is also inlined into the
  // vtable-referenced onRender(), duplicating ~290B of flash (the BLE build is tight).
  int  renderBody(Canvas& c, int x, int y, int w, int h) __attribute__((noinline));

  const uint8_t* targetForTest() const { return _pub; }
  int  lineCountForTest() const { return _view.count(); }
  const char* lineForTest(int i) const { return _view.lineForTest(i); }

private:
  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  bool     _pending = false;
  bool     _autoFetchDone = false; // guard: onShow fires fetch only on first tab activation
  uint32_t _statusStart = 0;
  PendingRequest _req;
  ScrollText     _view;
  Button         _refresh;

  void fetch();                                  // fire the request, show "Loading..."
  void showResult(const RepeaterStatusView& v);  // format rows into _view
};

StatusApplet& statusApplet();

}  // namespace mishmesh
