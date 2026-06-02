// mishmesh/widgets/ChatMenu.cpp
#include <mishmesh/widgets/ChatMenu.h>

namespace mishmesh {

ChatMenu::Result ChatMenu::activate(MessagesService* svc, const char*& toast) {
  toast = nullptr;
  if (!svc) return Result::None;
  switch (_menu.selected()) {
    case 0: svc->clearConvo(_key);  toast = "Chat cleared";  return Result::Cleared;
    case 1: svc->markUnread(_key);  toast = "Marked unread"; return Result::None;
    default: svc->deleteConvo(_key); toast = "Chat deleted";  return Result::Deleted;
  }
}

}  // namespace mishmesh
