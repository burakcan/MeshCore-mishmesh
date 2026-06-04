// mishmesh/widgets/ChatMenu.cpp
#include <mishmesh/widgets/ChatMenu.h>

namespace mishmesh {

ChatMenu::Result ChatMenu::activate(MessagesService* svc, const char*& toast) {
  toast = nullptr;
  _svc = svc;
  if (!svc) return Result::None;
  switch (_menu.selected()) {
    case 1: svc->markUnread(_key); toast = "Marked unread"; return Result::None;
    case 0: _confirm.configure("Clear this chat?");   _confirming = true; return Result::None;
    default: _confirm.configure("Delete this chat?"); _confirming = true; return Result::None;
  }
}

bool ChatMenu::onInput(InputEvent ev) {
  if (!_confirming) return _menu.onInput(ev);

  if (_confirm.onInput(ev)) {
    ConfirmResult r = _confirm.result();
    if (r == ConfirmResult::Confirmed) {
      if (_menu.selected() == 0) {        // Clear chat
        if (_svc) _svc->clearConvo(_key);
        _toast = "Chat cleared"; _pending = Result::Cleared;
      } else {                            // Delete chat
        if (_svc) _svc->deleteConvo(_key);
        _toast = "Chat deleted"; _pending = Result::Deleted;
      }
      _confirming = false; _confirm.reset();
    } else if (r == ConfirmResult::Cancelled) {
      _confirming = false; _confirm.reset();   // back to the action list, nothing done
    }
  }
  return true;   // swallow all input while the dialog is up
}

}  // namespace mishmesh
