// mishmesh/applets/RepeaterTelemetryApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/TelemetryDialog.h>

namespace mishmesh {

// Read-on-open telemetry view for a repeater: fires requestTelemetry, polls telemetrySeq,
// renders the decoded fields via the existing TelemetryDialog. Launch-only. Host-safe.
class RepeaterTelemetryApplet : public Applet {
public:
  RepeaterTelemetryApplet() : Applet("Telemetry") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool doneForTest() const { return _done; }
private:
  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  uint32_t _startSeq = 0, _startMs = 0;
  bool     _done = false;
  TelemetryDialog _dlg;
  static const uint32_t TELEM_TIMEOUT_MS = 15000;
};

RepeaterTelemetryApplet& repeaterTelemetryApplet();

}  // namespace mishmesh
