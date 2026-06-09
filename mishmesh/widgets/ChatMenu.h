// mishmesh/widgets/ChatMenu.h
#pragma once
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/core/MessagesService.h>

namespace mishmesh {

// Shared chat-level action menu (Region / Clear chat / Mark unread / Delete chat).
// Used both inline (thread Settings tab) and as an overlay (chat-list long-press),
// so the item list and the action logic live in exactly one place. The caller owns
// placement and what to do with the Result (open region editor / switch tab / pop /
// refresh).
//
// The destructive actions (Clear / Delete) route through an embedded confirm
// dialog so both call sites get the same guard for free. Flow per Select:
//   activate() -> Region returns EditRegion (caller opens the keypad), Mark unread
//   runs now (Result), Clear/Delete arm the dialog (confirming()==true). While
//   confirming, feed input to onInput() and poll takeResult() — it yields the
//   completed Result once the user confirms.
//
// The Region row's value is supplied by the caller (it owns the per-chat region
// lookup) via setRegion(); the widget itself just displays it.
class ChatMenu {
public:
  enum class Result { None, Cleared, Deleted, EditRegion };

  ChatMenu() { _model.region = _region; }   // point the model at our buffer once
  void setTarget(const ConvoKey& k) { _key = k; }
  // Shows `name` in the Region row's value column, or "None" when empty/null.
  void setRegion(const char* name) {
    if (name && name[0]) { strncpy(_region, name, sizeof(_region) - 1); _region[sizeof(_region) - 1] = 0; }
    else strcpy(_region, "None");
  }
  void reset() {
    _menu.setModel(&_model); _menu.setRowHeight(ROW_H); _menu.resetSelection();
    _confirming = false; _confirm.reset(); _pending = Result::None; _toast = nullptr;
  }
  bool needsAnimation() const { return _menu.needsAnimation(); }
  void draw(Canvas& c, int x, int y, int w, int h) {
    _menu.setModel(&_model); _menu.setRowHeight(ROW_H); _menu.draw(c, x, y, w, h);
  }
  int selected() const { return _menu.selected(); }

  // True while the confirm dialog is up; the caller should draw it via
  // drawConfirm() (over the full screen) and keep feeding input to onInput().
  bool confirming() const { return _confirming; }
  void drawConfirm(Canvas& c, int x, int y, int w, int h) { _confirm.draw(c, x, y, w, h); }

  // Routes input to the confirm dialog while armed, otherwise to the list menu.
  // Resolving the dialog runs the action and latches the Result for takeResult().
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
  MessagesService* _svc = nullptr;   // remembered from activate() for confirm resolution
  bool _confirming = false;
  Result _pending = Result::None;    // latched until takeResult()
  const char* _toast = nullptr;
  char _region[32] = "None";         // region row value (caller-supplied via setRegion)
  struct Model : ListModel {
    const char* region = "None";     // points at the owner's _region buffer
    int count() const override { return 4; }
    const char* label(int i) const override {
      if (i == 0) return "Region";
      if (i == 1) return "Clear chat";
      if (i == 2) return "Mark unread";
      return "Delete chat";
    }
    const char* value(int i) const override { return i == 0 ? region : nullptr; }
  } _model;
};

}  // namespace mishmesh
