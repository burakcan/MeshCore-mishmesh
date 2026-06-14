// mishmesh/applets/NotificationApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>

namespace mishmesh {

// Full-screen "new message" banner, raised by the notification router when a
// message lands while the device is idle - asleep, or sitting on the home screen.
// Back clears it, Select opens the chat, and it auto-dismisses after a few seconds
// if left untouched. A reused singleton (see notificationApplet()); the subject
// message is copied in via raise() so the banner survives later store mutations.
class NotificationApplet : public Applet {
public:
  NotificationApplet() : Applet("Notify") {}

  void raise(MessagesService* svc, const ConvoKey& key);

  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // test seams
  const char* nameForTest() const { return _name; }
  const char* senderForTest() const { return _sender; }
  const char* previewForTest() const { return _preview; }
  uint16_t    otherUnreadForTest() const { return _otherUnread; }

private:
  AppletHost*      _host = nullptr;
  MessagesService* _svc  = nullptr;
  ConvoKey         _key{};
  bool             _isChannel = false;
  uint16_t         _otherUnread = 0;     // other chats (not _key) still unread
  uint32_t         _raisedAt = 0;        // 0 => stamp on next render (auto-dismiss base)
  char             _name[34] = {0};      // contact (DM) or channel (group) name
  char             _sender[34] = {0};    // per-message sender, for channels
  char             _preview[128] = {0};  // last message body
};

NotificationApplet& notificationApplet();

}  // namespace mishmesh
