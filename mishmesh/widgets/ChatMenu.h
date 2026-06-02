// mishmesh/widgets/ChatMenu.h
#pragma once
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/core/MessagesService.h>

namespace mishmesh {

// Shared chat-level action menu (Clear chat / Mark unread / Delete chat). Used
// both inline (thread Settings tab) and as an overlay (chat-list long-press), so
// the item list and the action logic live in exactly one place. The caller owns
// placement and what to do with the Result (switch tab / pop / refresh).
class ChatMenu {
public:
  enum class Result { None, Cleared, Deleted };

  void setTarget(const ConvoKey& k) { _key = k; }
  void reset() { _menu.setModel(&_model); _menu.setRowHeight(ROW_H); _menu.resetSelection(); }
  bool onInput(InputEvent ev) { return _menu.onInput(ev); }
  bool needsAnimation() const { return _menu.needsAnimation(); }
  void draw(Canvas& c, int x, int y, int w, int h) {
    _menu.setModel(&_model); _menu.setRowHeight(ROW_H); _menu.draw(c, x, y, w, h);
  }
  int selected() const { return _menu.selected(); }

  // Runs the selected action against svc. Sets `toast` to a caller-owned status
  // string (or nullptr) and returns what happened so the caller can pop/refresh.
  Result activate(MessagesService* svc, const char*& toast);

private:
  static const int ROW_H = 14;
  ConvoKey _key{};
  ListMenu _menu;
  struct Model : ListModel {
    int count() const override { return 3; }
    const char* label(int i) const override {
      if (i == 0) return "Clear chat";
      if (i == 1) return "Mark unread";
      return "Delete chat";
    }
  } _model;
};

}  // namespace mishmesh
