#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ChatMenu.h>
#include <mishmesh/applets/FormApplet.h>

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
  enum class NewAction : uint8_t { Message, CreatePrivate, JoinPrivate, JoinPublic, JoinHashtag };
  // test seams
  void setChannelNameForTest(const char* s);
  void setChannelKeyForTest(const char* s);
  int  selectedTabForTest() const { return _tab; }

private:
  void syncList();
  void openCreatePrivate();
  void openJoinPrivate();
  void openJoinHashtag();
  void openRegionEditor();                          // keypad for the long-pressed chat's region
  void refreshRegion();                             // pull current region into the menu
  static void onRegionDone(void* ctx, const char* text);
  bool applyResult(ChanResult res, const char* okToast);   // true => pop form
  static bool isHexKey(const char* s);
  static bool submitCreatePrivate(void* ctx);
  static bool submitJoinPrivate(void* ctx);
  static bool submitJoinHashtag(void* ctx);

  char _chName[32];
  char _chKey[33];   // 32 hex chars + NUL

  AppletHost*      _host = nullptr;
  MessagesService* _svc  = nullptr;
  AppServices*     _app  = nullptr;
  char             _battBuf[8] = {0};    // stable backing for the battery decoration
  TabBar           _tabs;
  ListMenu         _list;
  ChatMenu         _chatMenu;            // long-press chat-action overlay
  ConvoKey         _menuKey{};           // chat the overlay/region editor targets
  ConvoKey         _openedKey{};         // chat just opened, to reselect by key on return
  bool             _hasOpened = false;   // _openedKey is valid (consumed on next foreground)
  char             _regionBuf[31] = {0}; // backs keypad text while editing region
  bool             _menuOpen = false;
  int              _tab = 0;             // 0 = Chats, 1 = New, 2 = Settings
  bool             settingsTab() const { return _tab == 2; }   // matches Advert/Contacts applets

  struct ChatsModel : ListModel {
    MessagesService* svc = nullptr;
    int          count()       const override;
    const char*  label(int i)  const override;
    uint16_t     icon(int i)   const override;
    const char*  value(int i)  const override;   // unread badge "N"
  } _chats;

  struct NewModel : ListModel {
    MessagesService* svc = nullptr;
    int count() const override { return (svc && !svc->publicChannelJoined()) ? 5 : 4; }
    const char* label(int i) const override;
    uint16_t    icon(int i)  const override;
    NewAction   actionAt(int i) const;
  } _new;

};

MessagesApplet& messagesApplet();

}  // namespace mishmesh
