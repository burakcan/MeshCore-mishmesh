#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Per-chat notification drill-down (reached from ChatMenu's "Notifications" row).
// A single list: level radios on top (DMs: All/Mute; channels add "Mentions
// only"), then two non-radio value rows - "Sound" (pushes the shared
// SoundPickerApplet, per-chat mode) and "Screen wake" (pushes WakeOverrideApplet).
// Launch-only singleton.
class ChatNotifyApplet : public Applet, public ListModel {
public:
  ChatNotifyApplet();
  void setTarget(const ConvoKey& key, const char* name);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel: level rows (2 DM / 3 channel) + Sound row + Screen wake row.
  int count() const override { return (_isChannel ? 3 : 2) + 2; }
  const char* label(int i) const override;
  const char* value(int i) const override;        // Sound/Screen wake rows -> current setting
  bool isRadio(int i) const override;
  bool radioOn(int i) const override;

private:
  bool soundRow(int i) const { return i == (_isChannel ? 3 : 2); }
  bool wakeRow(int i) const { return i == (_isChannel ? 4 : 3); }
  NotifyLevel levelForRow(int row) const;

  AppServices*     _app = nullptr;
  MessagesService* _svc = nullptr;
  AppletHost*      _host = nullptr;
  ConvoKey         _key{};
  bool             _isChannel = false;
  NotifyLevel      _level = NotifyLevel::All;
  char             _name[44] = {0};
  ListMenu         _list;
  StatusBar        _bar;
};

ChatNotifyApplet& chatNotifyApplet();

}  // namespace mishmesh
