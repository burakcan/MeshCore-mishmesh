// mishmesh/applets/RepeaterRegionsApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Lists a repeater's regions (allow/deny flood) and adds/deletes/saves, over the CLI
// channel. Launch-only. Host-safe. No firmware changes.
class RepeaterRegionsApplet : public Applet {
public:
  static const int MAX_REGIONS = 16;
  static const int NAME_CAP = 32;

  RepeaterRegionsApplet() : Applet("Regions") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int  regionCountForTest() const { return _count; }
  const char* regionNameForTest(int i) const { return (i >= 0 && i < _count) ? _reg[i].name : ""; }
  bool regionAllowedForTest(int i) const { return (i >= 0 && i < _count) && _reg[i].allowed; }
  bool isLoadingForTest() const { return _phase == Phase::Loading; }
  bool saveRowIsButtonForTest() const { return _model.isButton(_count + 2); }

private:
  enum class Phase : uint8_t { Loading, List, Busy, Editing };
  enum class Op    : uint8_t { None, FetchAllowed, FetchDenied, Toggle, Add, Delete, Save };
  struct Region { char name[NAME_CAP]; bool allowed; };

  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppServices*     _app = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  Region   _reg[MAX_REGIONS];
  StatusBar _bar;
  int      _count = 0;
  Phase    _phase = Phase::Loading;
  Op       _op = Op::None;
  int      _toggleIdx = -1;
  uint32_t _startSeq = 0;
  char     _status[40] = {0};
  char     _scratch[NAME_CAP] = {0};
  bool     _kpDelete = false;
  PendingRequest _req;
  ListMenu _menu;

  struct Model : ListModel {
    RepeaterRegionsApplet* p = nullptr;
    int count() const override { return p->_count + 3; }        // regions + Add + Delete + Save
    const char* label(int i) const override;
    bool isToggle(int i) const override { return i < p->_count; }
    bool toggleState(int i) const override { return i < p->_count && p->_reg[i].allowed; }
    bool isButton(int i) const override { return i == p->_count + 2; }  // Save row
  } _model;

  void beginFetch();
  void fireCmd(Op op, const char* cmd, Phase ph);
  void parseList(const char* resp, bool allowed);
  static void onKeypadDone(void* ctx, const char* text);
};

RepeaterRegionsApplet& repeaterRegionsApplet();

}  // namespace mishmesh
