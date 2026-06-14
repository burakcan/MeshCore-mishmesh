#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Per-chat notification level: a single-select list reached from the ChatMenu's
// "Notifications" row. DMs show {All, Mute}; channels add "Mentions only". Select
// sets the level on the target chat (persisted via the service). Launch-only,
// like ContactPermissionsApplet.
class ChatNotifyApplet : public Applet, public ListModel {
public:
  ChatNotifyApplet();
  void setTarget(const ConvoKey& key, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel: 2 rows (DM) or 3 rows (channel), single-select radio — the chosen
  // level shows a check; moving the cursor doesn't move the mark.
  int count() const override { return _isChannel ? 3 : 2; }
  const char* label(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

private:
  NotifyLevel levelForRow(int row) const;   // maps row index -> NotifyLevel

  AppServices*     _app = nullptr;
  MessagesService* _svc = nullptr;
  ConvoKey         _key{};
  bool             _isChannel = false;
  NotifyLevel      _level = NotifyLevel::All;   // cached active level
  char             _name[44] = {0};             // top-bar title
  ListMenu         _list;
  StatusBar        _bar;
};

ChatNotifyApplet& chatNotifyApplet();

}  // namespace mishmesh
