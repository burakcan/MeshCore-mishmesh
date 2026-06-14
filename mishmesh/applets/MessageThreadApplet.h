// mishmesh/applets/MessageThreadApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ChatMenu.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/widgets/Button.h>

namespace mishmesh {

class MessageThreadApplet : public Applet {
public:
  MessageThreadApplet() : Applet("Thread") {}
  void setTarget(const ConvoKey& k, const char* fallbackName = nullptr);
  void composeOnOpen() { _composeOnOpen = true; }   // one-shot: open focused on the Write button
  void onStart(AppletContext&) override;
  void onForeground() override;
  void onBackground() override;
  void onStop() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int  focusedIndexForTest() const { return _focus; }
  const char* headerTitleForTest() const { return _titleBuf; }
  int  selectedTabForTest() const { return _tabs.selected(); }
  int  barRowForTest() const { return _barRow; }
  const char* msgMenuLabelForTest(int i) const { return _msgMenu.label(i); }
private:
  const char* resolveTitle() const;
  bool onConversationInput(InputEvent ev);   // tab 0: message nav / per-message menu
  int  blockHeight(Canvas& body, const MessageView& m) const;
  void layoutFocus(Canvas& body, int n);   // sets _focusTop/_focusBot/_contentH
  void adjustScroll();                      // clamps _scrollY to keep focus visible
  void drawMessage(Canvas& body, const MessageView& m, int top, bool focused) const;
  void drawActionBar(Canvas& bar);   // two stacked Write/Quick buttons, focus per _barRow
  void startCompose(const char* seed = nullptr);
  static void onComposeDone(void* ctx, const char* text);
  void openRegionEditor();                          // keypad for the chat's region
  void refreshRegion();                             // pull current region into the menu
  static void onRegionDone(void* ctx, const char* text);

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
  bool             _unseenBelow = false; // a message arrived below the fold -> show a chevron
  int              _prevCount = 0;    // message count last frame, to detect new arrivals
  uint32_t         _lastMsgTime = 0;  // newest message's time; detects arrivals even at the cap
  uint32_t         _lastSeq = 0;
  bool             _menuOpen = false; // per-message action menu overlay (Reply/Delete/Path)
  char             _titleBuf[40] = {0};  // stable backing for the conversation tab label
  char             _fallbackName[32] = {0};   // header name when no conversation exists yet
  char             _battBuf[8] = {0};    // stable backing for the battery decoration

  TabBar    _tabs;       // [0] conversation, [1] Settings
  ChatMenu  _chatMenu;   // shared chat-action menu, shown inline on the Settings tab
  ListMenu  _menu;       // per-message action overlay
  struct MsgMenuModel : ListModel {
    bool hasPath = false;
    bool repeats = false;   // outbound channel -> "Heard Repeats" instead of "View Path"
    int count() const override { return hasPath ? 3 : 2; }
    const char* label(int i) const override {
      if (i == 0) return "Reply";
      if (i == 1) return "Delete";
      return repeats ? "Heard Repeats" : "View Path";
    }
  } _msgMenu;
  int       _barRow = -1;     // -1 = on a message; 0 = Write; 1 = Quick
  bool      _composeOnOpen = false;   // one-shot: focus the Write button on next onStart
  Button    _btn;             // reused to draw each stacked action button in turn
  char _composeBuf[KeypadApplet::KP_MAX + 1];   // backs keypad text during compose
  char _regionBuf[31];                          // backs keypad text while editing region
};

MessageThreadApplet& messageThreadApplet();

} // namespace mishmesh
