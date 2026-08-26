#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class Canvas;

// Per-chat screen-wake override (Default / On / Off). Default follows the global
// "Wake screen on message" setting. Writes straight to MessagesService::setChatWake; no
// staging. Launch-only singleton, reached from ChatNotifyApplet's "Screen wake"
// row - mirrors PathHashApplet.
class WakeOverrideApplet : public Applet, public ListModel {
public:
  WakeOverrideApplet();
  void setTarget(const ConvoKey& key, const char* name);

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int count() const override { return 3; }             // Default / On / Off
  const char* label(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

  void selectRowForTest(int i) { _list.setSelected(i); }

private:
  AppServices*     _app = nullptr;
  MessagesService* _svc = nullptr;
  ConvoKey         _key{};
  char             _name[44] = {0};
  ListMenu  _list;
  StatusBar _bar;
};

WakeOverrideApplet& wakeOverrideApplet();

}  // namespace mishmesh
