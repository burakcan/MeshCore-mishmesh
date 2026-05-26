#pragma once

#include <string.h>

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

class TelemetryApplet : public Applet, public ListModel {
  AppletHost*      _host;
  ContactsService* _svc;
  uint8_t          _pubkey[6];
  uint32_t         _startSeq;
  uint32_t         _startMs;
  bool             _arrived;
  int              _scrollTop;   // first visible field row (read-only list, scrollable)
  TelemetryView    _view;
  mutable char     _rowLabel[24];
  mutable char     _rowValue[24];
public:
  TelemetryApplet();
  void setTarget(const uint8_t* pubKey) { memcpy(_pubkey, pubKey, 6); }
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int count() const override { return _arrived ? _view.count : 0; }
  const char* label(int i) const override;
  const char* value(int i) const override;
};

TelemetryApplet& telemetryApplet();
void telemetrySetTarget(const uint8_t* pubKey);

}  // namespace mishmesh
