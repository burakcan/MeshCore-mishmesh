#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// Active node discovery, mirroring the mobile app's "Discover Nodes" screen. Row 0
// "Discover Repeaters" and row 1 "Discover Sensors" are buttons that fire a zero-hop
// NODE_DISCOVER_REQ; rows 2.. are the direct-neighbour replies heard this session.
class DiscoverApplet : public Applet {
public:
  class Model : public ListModel {
    ContactsService* _svc = nullptr;
  public:
    void bind(ContactsService* svc) { _svc = svc; }
    int count() const override { return 2 + (_svc ? _svc->discoverResultCount() : 0); }
    const char* label(int i) const override;
    uint16_t icon(int i) const override;
    const char* value(int i) const override;
    bool isButton(int i) const override { return i < 2; }
  };

  DiscoverApplet();
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  Model& model() { return _model; }
  bool scanning() const { return _svc && _svc->discoverScanning(); }

private:
  void startScan(uint8_t advTypeMask);

  AppServices*     _app = nullptr;
  ContactsService* _svc = nullptr;
  AppletHost*      _host = nullptr;
  Model            _model;
  ListMenu         _list;
  uint32_t         _seenSeq = 0;
};

DiscoverApplet& discoverApplet();

}  // namespace mishmesh
