// mishmesh/widgets/ChatMenu.h
#pragma once
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/StepperDialog.h>
#include <mishmesh/core/MessagesService.h>

namespace mishmesh {

// Shared chat-level action menu (Region / Clear chat / Mark unread / Delete chat).
// Used both inline (thread Settings tab) and as an overlay (chat-list long-press),
// so the item list and the action logic live in exactly one place. The caller owns
// placement and what to do with the Result (open region editor / switch tab / pop /
// refresh).
//
// Two embedded modals keep their flows in one place: Clear/Delete route through a
// confirm dialog, and Notifications opens a value stepper (All/Mentions/Mute).
// Flow per Select: activate() -> Region returns EditRegion (caller opens the
// keypad), Mark unread runs now (Result), Clear/Delete arm the confirm dialog,
// Notifications arms the stepper. While a modal is up (modalActive()==true) the
// caller draws it via drawModal() and keeps feeding input to onInput(); the confirm
// dialog's outcome is polled via takeResult(), while the stepper writes the chosen
// level itself and updates the Notifications row.
//
// The Region row's value is supplied by the caller via setRegion(); the
// Notifications row's via setNotifyLabel() (the widget keeps it in sync after a
// stepper edit). The widget itself just displays them.
class ChatMenu {
public:
  enum class Result { None, Cleared, Deleted, EditRegion };

  ChatMenu() { _model.region = _region; _model.notify = _notify; }   // point the model at our buffers once
  void setTarget(const ConvoKey& k) { _key = k; }
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
    _confirming = false; _confirm.reset(); _stepping = false; _stepper.reset();
    _pending = Result::None; _toast = nullptr;
  }
  bool needsAnimation() const { return _menu.needsAnimation(); }
  void draw(Canvas& c, int x, int y, int w, int h) {
    _menu.setModel(&_model); _menu.setRowHeight(ROW_H); _menu.draw(c, x, y, w, h);
  }
  int selected() const { return _menu.selected(); }

  // True while either modal (confirm dialog or notify stepper) is up; the caller
  // draws it via drawModal() (full screen) and keeps feeding input to onInput().
  bool modalActive() const { return _confirming || _stepping; }
  void drawModal(Canvas& c, int x, int y, int w, int h) {
    if (_stepping) _stepper.draw(c, x, y, w, h);
    else           _confirm.draw(c, x, y, w, h);
  }

  // Routes input to the active modal while armed, otherwise to the list menu.
  // Resolving the confirm dialog latches a Result for takeResult(); resolving the
  // stepper writes the chosen notify level and refreshes the row in place.
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
  StepperDialog _stepper;
  MessagesService* _svc = nullptr;   // remembered from activate() for modal resolution
  bool _confirming = false;
  bool _stepping = false;
  Result _pending = Result::None;    // latched until takeResult()
  const char* _toast = nullptr;
  char _region[32] = "None";         // region row value (caller-supplied via setRegion)
  char _notify[12] = "All";          // Notifications row value (caller-supplied)
  struct Model : ListModel {
    const char* region = "None";     // points at the owner's _region buffer
    const char* notify = "All";      // points at the owner's _notify buffer
    int count() const override { return 5; }
    const char* label(int i) const override {
      if (i == 0) return "Region";
      if (i == 1) return "Notifications";
      if (i == 2) return "Clear chat";
      if (i == 3) return "Mark unread";
      return "Delete chat";
    }
    const char* value(int i) const override {
      if (i == 0) return region;
      if (i == 1) return notify;
      return nullptr;
    }
  } _model;
};

}  // namespace mishmesh
