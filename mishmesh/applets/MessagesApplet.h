#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ChatMenu.h>

namespace mishmesh {

class MessagesApplet : public Applet {
public:
  MessagesApplet() : Applet("Messages") {}
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int  visibleRowCountForTest() const;   // test hook
  bool menuOpenForTest() const { return _menuOpen; }

private:
  void syncList();

  AppletHost*      _host = nullptr;
  MessagesService* _svc  = nullptr;
  AppServices*     _app  = nullptr;
  char             _battBuf[8] = {0};    // stable backing for the battery decoration
  TabBar           _tabs;
  ListMenu         _list;
  ChatMenu         _chatMenu;            // long-press chat-action overlay
  bool             _menuOpen = false;
  int              _tab = 0;             // 0 = Chats, 1 = New

  struct ChatsModel : ListModel {
    MessagesService* svc = nullptr;
    int          count()       const override;
    const char*  label(int i)  const override;
    uint16_t     icon(int i)   const override;
    const char*  value(int i)  const override;   // unread badge "N"
  } _chats;

  struct NewModel : ListModel {
    int          count()       const override { return 3; }
    const char*  label(int i)  const override;
    uint16_t     icon(int i)   const override;
  } _new;
};

MessagesApplet& messagesApplet();

}  // namespace mishmesh
