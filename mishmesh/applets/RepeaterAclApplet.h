// mishmesh/applets/RepeaterAclApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/PendingRequest.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Lists a repeater's ACL (binary GET_ACCESS_LIST) and adds/changes/removes clients via
// setperm (CLI). Add opens the contact picker. Launch-only. Host-safe.
class RepeaterAclApplet : public Applet {
public:
  enum class Phase : uint8_t { Loading, List, Level, Picking, Busy };

  RepeaterAclApplet() : Applet("Access") {}
  void  setTarget(const uint8_t* pubKey, const char* name);
  void  onStart(AppletContext& ctx) override;
  void  onForeground() override;
  int   onRender(Canvas& c) override;
  bool  onInput(InputEvent ev) override;
  int   entryCountForTest() const { return _view.count; }
  int   entryPermsForTest(int i) const { return (i >= 0 && i < _view.count) ? _view.entries[i].perms : -1; }
  Phase phaseForTest() const { return _phase; }

private:
  enum class Op    : uint8_t { None, Fetch, Setperm };

  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppServices*     _app = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  AccessListView _view{};
  StatusBar _bar;
  uint8_t  _target[6] = {0};       // pubkey being edited (entry or picked)
  bool     _pickReady = false;
  Phase    _phase = Phase::Loading;
  Op       _op = Op::None;
  uint32_t _startSeq = 0;
  char     _status[40] = {0};
  PendingRequest _req;
  ListMenu _menu;
  ListMenu _levelMenu;

  struct ListModel_ : ListModel {   // entries + "Add user"
    RepeaterAclApplet* p = nullptr;
    int count() const override { return p->_view.count + 1; }
    const char* label(int i) const override;
  } _model;

  void beginFetch();
  void fireSetperm(uint8_t perms);
  static void onPicked(void* ctx, const uint8_t* pubKey);
};

RepeaterAclApplet& repeaterAclApplet();

}  // namespace mishmesh
