// mishmesh/applets/MessageThreadApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ChatMenu.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/widgets/Button.h>
#include <mishmesh/core/QuickReplyStore.h>

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
  // An open conversation stays put on wake - don't reset to home mid-chat.
  bool keepOnWake() const override { return true; }
  int  focusedIndexForTest() const { return _focus; }
  const char* headerTitleForTest() const { return _titleBuf; }
  int  selectedTabForTest() const { return _tabs.selected(); }
  int  barRowForTest() const { return _barRow; }
  // Metadata string shown beneath an outbound bubble (Sent / Retrying N/5 /
  // Delivered / Failed / Heard). Exposed for host tests.
  static void statusLabel(const MessageView& m, char* buf, int cap);
  const char* msgMenuLabelForTest(int i) const { return _msgMenu.label(i); }
private:
  const char* resolveTitle() const;
  bool onConversationInput(InputEvent ev);   // tab 0: message nav / per-message menu
  int  blockHeight(Canvas& body, const MessageView& m) const;
  void layoutFocus(Canvas& body, int n);   // sets _focusTop/_focusBot/_contentH
  void ensureHeights(Canvas& body, int n); // fill _blkH[]/_contentH from cache or (re)measure
  void computeFocusSpan(int n);            // _focusTop/_focusBot from cached heights + _focus
  void adjustScroll();                      // clamps _scrollY to keep focus visible
  // blockH is the message's cached height (_blkH[i]); drawMessage reuses it for
  // the clip region and to derive the outbound bubble height instead of
  // re-measuring (ensureHeights already ran the measure pass).
  void drawMessage(Canvas& body, const MessageView& m, int top, int blockH, bool focused) const;
  void msgStamp(const MessageView& m, char* out, uint16_t cap) const;   // "14:09" / "1 Jul 14:09"
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
  bool             _hasScrollbar = false; // sticky overflow state, keeps _contentW stable frame-to-frame
  // Per-message block-height cache. Without it, layoutFocus + the draw loop
  // re-measure (and, for flash-resident history, re-page from flash) EVERY
  // message EVERY frame - O(n^2) flash reads for a long chat, which freezes
  // scrolling. Rebuild only when the message set or width changes; a lone new
  // arrival appends a single entry.
  static const int LAY_MAX = PER_CHAT_CAP;
  uint16_t         _blkH[LAY_MAX] = {0};   // cached block height per message index
  int              _layN = -1;             // n at last cache fill (-1 = empty)
  int              _layW = -1;             // _contentW at last fill
  uint32_t         _layNewest = 0xFFFFFFFFu; // this chat's newest-message time at last fill
                                             // (count+newest identify THIS chat's message set,
                                             //  so other chats' activity and status-only updates
                                             //  don't trigger a needless remeasure)
  ConvoKey         _layKey{};              // chat the cache belongs to
  bool             _layValid = false;
  bool             _pinBottom = false;// on next render, scroll to the newest message
  bool             _unseenBelow = false; // a message arrived below the fold -> show a chevron
  int              _prevCount = 0;    // message count last frame, to detect new arrivals
  uint32_t         _lastMsgTime = 0;  // newest message's time; detects arrivals even at the cap
  uint32_t         _lastSeq = 0;
  bool             _menuOpen = false; // per-message action menu overlay (Reply/Delete/Path)
  bool             _qrOpen = false;   // quick-replies picker overlay (twin of _menuOpen)
  char             _titleBuf[40] = {0};  // stable backing for the conversation tab label
  char             _fallbackName[32] = {0};   // header name when no conversation exists yet

  TabBar    _tabs;       // [0] conversation, [1] Settings
  ChatMenu  _chatMenu;   // shared chat-action menu, shown inline on the Settings tab
  ListMenu  _menu;       // per-message action overlay
  struct MsgMenuModel : ListModel {
    bool hasPath = false;
    bool repeats = false;     // outbound channel -> "Heard Repeats" instead of "View Path"
    bool canResend = false;   // failed DM / 0-heard channel -> offer "Resend"
    enum Action : uint8_t { Reply, Resend, Delete, Path };
    // Visible order: Reply, [Resend], Delete, [Path].
    Action actionAt(int i) const {
      Action seq[4]; int n = 0;
      seq[n++] = Reply;
      if (canResend) seq[n++] = Resend;
      seq[n++] = Delete;
      if (hasPath) seq[n++] = Path;
      return (i >= 0 && i < n) ? seq[i] : Delete;
    }
    int count() const override { return 1 + (canResend ? 1 : 0) + 1 + (hasPath ? 1 : 0); }
    const char* label(int i) const override {
      switch (actionAt(i)) {
        case Reply:  return "Reply";
        case Resend: return "Resend";
        case Delete: return "Delete";
        default:     return repeats ? "Heard Repeats" : "View Path";
      }
    }
  } _msgMenu;
  struct QuickReplyModel : ListModel {
    int count() const override { return quickReplyStore().count(); }
    const char* label(int i) const override { return quickReplyStore().text(i); }
  } _qrModel;
  void openQuickReplies();
  int       _barRow = -1;     // -1 = on a message; 0 = Write; 1 = Quick
  bool      _composeOnOpen = false;   // one-shot: focus the Write button on next onStart
  Button    _btn;             // reused to draw each stacked action button in turn
  char _composeBuf[KeypadApplet::KP_MAX + 1];   // backs keypad text during compose
  char _regionBuf[31];                          // backs keypad text while editing region
};

MessageThreadApplet& messageThreadApplet();

} // namespace mishmesh
