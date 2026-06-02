// mishmesh/applets/MessageThreadApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ChatMenu.h>

namespace mishmesh {

class MessageThreadApplet : public Applet {
public:
  MessageThreadApplet() : Applet("Thread") {}
  void setTarget(const ConvoKey& k) { _key = k; }
  void onStart(AppletContext&) override;
  void onForeground() override;
  void onBackground() override;
  void onStop() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int  focusedIndexForTest() const { return _focus; }
  int  selectedTabForTest() const { return _tabs.selected(); }
private:
  const char* resolveTitle() const;
  bool onConversationInput(InputEvent ev);   // tab 0: message nav / per-message menu
  int  blockHeight(Canvas& body, const MessageView& m) const;
  void layoutFocus(Canvas& body, int n);   // sets _focusTop/_focusBot/_contentH
  void adjustScroll();                      // clamps _scrollY to keep focus visible
  void drawMessage(Canvas& body, const MessageView& m, int top, bool focused) const;

  AppletHost*      _host = nullptr;
  MessagesService* _svc = nullptr;
  AppServices*     _app = nullptr;
  ConvoKey         _key{};
  int              _focus = 0;        // message index (0 = oldest)
  int              _scrollTarget = 0; // committed scroll destination (content px)
  int              _scrollY = 0;      // eased current scroll, glides toward target
  bool             _animReady = false;// false => snap to target on next draw (open)
  int              _bodyH = 0;        // last body viewport height (for input math)
  int              _contentW = 0;     // usable width (full, or minus the scrollbar gutter)
  int              _focusTop = 0;     // content px of the focused message's top/bottom
  int              _focusBot = 0;
  int              _contentH = 0;
  bool             _pinBottom = false;// on next render, scroll to the newest message
  uint32_t         _lastSeq = 0;
  bool             _menuOpen = false; // per-message action menu overlay (Reply/Delete/Path)
  char             _titleBuf[40] = {0};  // stable backing for the conversation tab label
  char             _battBuf[8] = {0};    // stable backing for the battery decoration

  TabBar    _tabs;       // [0] conversation, [1] Settings
  ChatMenu  _chatMenu;   // shared chat-action menu, shown inline on the Settings tab
  ListMenu  _menu;       // per-message action overlay
  struct MsgMenuModel : ListModel {
    bool hasPath = false;
    int count() const override { return hasPath ? 3 : 2; }
    const char* label(int i) const override {
      if (i == 0) return "Reply";
      if (i == 1) return "Delete";
      return "View Path";
    }
  } _msgMenu;
};

MessageThreadApplet& messageThreadApplet();

} // namespace mishmesh
