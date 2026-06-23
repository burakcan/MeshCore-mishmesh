// mishmesh/widgets/ChatMenu.h
#pragma once
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/core/MessagesService.h>

namespace mishmesh {

// Shared chat-level action menu (Region / Notifications / Clear chat / Mark unread /
// Delete chat). Used both inline (thread Settings tab) and as an overlay (chat-list
// long-press), so the item list and the action logic live in exactly one place. The
// caller owns placement and what to do with the Result.
//
// Flow per Select: activate() -> Region returns EditRegion (caller opens the keypad),
// Notifications returns EditNotify (caller pushes ChatNotifyApplet), Mark unread runs
// now, Clear/Delete arm the confirm dialog. While the confirm dialog is up
// (modalActive()==true) the caller draws it via drawModal() (full screen) and keeps
// feeding input to onInput(); the outcome is polled via takeResult().
//
// The Region row's value is supplied by the caller via setRegion(); the Notifications
// row's via setNotifyLabel(). The widget itself just displays them.
class ChatMenu {
public:
  enum class Result { None, Cleared, Deleted, EditRegion, EditNotify, Share };

  ChatMenu() { _model.region = _region; _model.notify = _notify; }   // point the model at our buffers once
  // Channels (k.type==1) get an extra "Share" row; direct chats don't.
  void setTarget(const ConvoKey& k) { _key = k; _model.isChannel = (k.type == 1); }
  // Shows `name` in the Region row's value column, or "None" when empty/null.
  void setRegion(const char* name) {
    if (name && name[0]) { strncpy(_region, name, sizeof(_region) - 1); _region[sizeof(_region) - 1] = 0; }
    else strcpy(_region, "None");
  }
  // Shows `s` in the Notifications row's value column ("All"/"Mentions"/"Mute").
  void setNotifyLabel(const char* s) {
    strncpy(_notify, s ? s : "All", sizeof(_notify) - 1);
    _notify[sizeof(_notify) - 1] = 0;
  }
  void reset() {
    _menu.setModel(&_model); _menu.setRowHeight(ROW_H); _menu.resetSelection();
    _confirming = false; _confirm.reset();
    _pending = Result::None; _toast = nullptr;
  }
  bool needsAnimation() const { return _menu.needsAnimation(); }
  void draw(Canvas& c, int x, int y, int w, int h) {
    _menu.setModel(&_model); _menu.setRowHeight(ROW_H); _menu.draw(c, x, y, w, h);
  }
  int selected() const { return _menu.selected(); }

  // True while the confirm dialog is up; the caller draws it via drawModal() (full
  // screen) and keeps feeding input to onInput().
  bool modalActive() const { return _confirming; }
  void drawModal(Canvas& c, int x, int y, int w, int h) {
    _confirm.draw(c, x, y, w, h);
  }

  // Routes input to the confirm dialog while armed, otherwise to the list menu.
  // Resolving the confirm dialog latches a Result for takeResult().
  bool onInput(InputEvent ev);

  // Begins the selected action: non-destructive ones run immediately and return
  // their Result; Clear/Delete arm the confirm dialog and return None (poll
  // takeResult() after confirmation). `toast` is set for the immediate cases.
  Result activate(MessagesService* svc, const char*& toast);

  // Yields a completed destructive Result (and its toast) exactly once.
  Result takeResult(const char*& toast) {
    toast = _toast; Result r = _pending;
    _pending = Result::None; _toast = nullptr;
    return r;
  }

private:
  static const int ROW_H = 14;
  ConvoKey _key{};
  ListMenu _menu;
  ConfirmDialog _confirm;
  MessagesService* _svc = nullptr;   // remembered from activate() for modal resolution
  bool _confirming = false;
  Result _pending = Result::None;    // latched until takeResult()
  const char* _toast = nullptr;
  char _region[32] = "None";         // region row value (caller-supplied via setRegion)
  char _notify[12] = "All";          // Notifications row value (caller-supplied)
  struct Model : ListModel {
    const char* region = "None";     // points at the owner's _region buffer
    const char* notify = "All";      // points at the owner's _notify buffer
    bool isChannel = false;          // channels expose the extra "Share" row
    enum Action : uint8_t { ARegion, ANotify, AShare, AClear, AMarkUnread, ADelete };
    // Visible order: Region, Notifications, [Share], Clear, Mark unread, Delete.
    Action actionAt(int i) const {
      Action seq[6]; int n = 0;
      seq[n++] = ARegion; seq[n++] = ANotify;
      if (isChannel) seq[n++] = AShare;
      seq[n++] = AClear; seq[n++] = AMarkUnread; seq[n++] = ADelete;
      return (i >= 0 && i < n) ? seq[i] : ADelete;
    }
    int count() const override { return isChannel ? 6 : 5; }
    const char* label(int i) const override {
      switch (actionAt(i)) {
        case ARegion:     return "Region";
        case ANotify:     return "Notifications";
        case AShare:      return "Share";
        case AClear:      return "Clear chat";
        case AMarkUnread: return "Mark unread";
        default:          return "Delete chat";
      }
    }
    const char* value(int i) const override {
      switch (actionAt(i)) {
        case ARegion: return region;
        case ANotify: return notify;
        default:      return nullptr;
      }
    }
  } _model;
};

}  // namespace mishmesh
