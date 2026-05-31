// mishmesh/applets/MessageThreadApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

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
private:
  const char* resolveTitle() const;
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
  bool             _menuOpen = false;
  bool             _chatMenu = false;

  StatusBar _status;
  ListMenu  _menu;
  struct MsgMenuModel : ListModel {
    bool hasPath = false;
    int count() const override { return hasPath ? 3 : 2; }
    const char* label(int i) const override {
      if (i == 0) return "Reply";
      if (i == 1) return "Delete";
      return "View Path";
    }
  } _msgMenu;
  struct ChatMenuModel : ListModel {
    int count() const override { return 3; }
    const char* label(int i) const override {
      if (i == 0) return "Clear chat";
      if (i == 1) return "Mark unread";
      return "New message";
    }
  } _chatMenuModel;
};

MessageThreadApplet& messageThreadApplet();

} // namespace mishmesh
