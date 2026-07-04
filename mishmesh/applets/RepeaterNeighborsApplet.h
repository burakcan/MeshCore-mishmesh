// mishmesh/applets/RepeaterNeighborsApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Read-on-open zero-hop neighbours view: fires the CLI `neighbors` command and shows the
// multi-line reply (hex:secs:snr, or -none-) in a wrapped ScrollText. Launch-only. Host-safe.
class RepeaterNeighborsApplet : public Applet {
public:
  RepeaterNeighborsApplet() : Applet("Neighbors") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int  lineCountForTest() const { return _view.count(); }
  const char* lineForTest(int i) const { return _view.lineForTest(i); }
private:
  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppServices*     _app = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  uint32_t _startSeq = 0;
  bool     _pending = false;
  PendingRequest _req;
  ScrollText     _view;
  StatusBar      _bar;
  void appendMultiline(const char* s);
};

RepeaterNeighborsApplet& repeaterNeighborsApplet();

}  // namespace mishmesh
