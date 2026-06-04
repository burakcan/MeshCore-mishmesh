#include "MessagesApplet.h"
#include "MessageThreadApplet.h"
#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/widgets/StatusBar.h>   // batteryPercent()
#include <mishmesh/text/Fonts.h>
#include <cstdio>

namespace mishmesh {

// ---- ChatsModel ----

int MessagesApplet::ChatsModel::count() const {
  return svc ? svc->convoCount() : 0;
}

const char* MessagesApplet::ChatsModel::label(int i) const {
  static char buf[40];
  ConvoView v;
  if (!svc || !svc->getConvo(i, v)) return "";
  snprintf(buf, sizeof(buf), "%s", v.name);
  return buf;
}

uint16_t MessagesApplet::ChatsModel::icon(int i) const {
  ConvoView v;
  if (!svc || !svc->getConvo(i, v)) return 0;
  return (uint16_t)(v.isChannel ? Icon::Users : Icon::User);
}

const char* MessagesApplet::ChatsModel::value(int i) const {
  static char buf[8];
  ConvoView v;
  if (!svc || !svc->getConvo(i, v) || v.unread == 0) return nullptr;
  snprintf(buf, sizeof(buf), "%u", v.unread);
  return buf;
}

// ---- NewModel ----

const char* MessagesApplet::NewModel::label(int i) const {
  switch (i) {
    case 0:  return "New message";
    case 1:  return "New group";
    default: return "Join channel";
  }
}

uint16_t MessagesApplet::NewModel::icon(int i) const {
  switch (i) {
    case 0:  return (uint16_t)Icon::Message;
    case 1:  return (uint16_t)Icon::Users;
    default: return (uint16_t)Icon::Comment;
  }
}

// ---- MessagesApplet ----

void MessagesApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc  = ctx.messages;
  _app  = ctx.app;
  _chats.svc = _svc;
  _tabs.clear();
  _tabs.addTab("Chats", (uint16_t)Icon::Message);
  _tabs.addTab("New", (uint16_t)Icon::Plus);
  _tab = 0;
  syncList();
}

void MessagesApplet::onForeground() {
  _chats.svc = _svc;
  syncList();
}

void MessagesApplet::syncList() {
  _list.setModel(_tab == 0 ? (ListModel*)&_chats : (ListModel*)&_new);
  _list.setRowHeight(14);
  _list.setEmptyText(_tab == 0 ? "No messages yet" : "");
}

int MessagesApplet::visibleRowCountForTest() const {
  return _tab == 0 ? _chats.count() : _new.count();
}

int MessagesApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int barH = 13;
  snprintf(_battBuf, sizeof(_battBuf), "%d%%", batteryPercent(_app ? _app->batteryMillivolts() : 0));
  _tabs.setDecoration(_battBuf);
  _tabs.draw(c, 0, 0, w, barH);
  int bodyY = barH + 1;
  int bodyH = h - bodyY;
  _list.draw(c, 0, bodyY, w, bodyH);

  if (_menuOpen) {
    Canvas box = drawModalChrome(c);   // bare box over the live list
    _chatMenu.draw(box, 2, 2, box.width() - 4, box.height() - 4);
    if (_chatMenu.confirming()) _chatMenu.drawConfirm(c, 0, 0, w, h);   // full-screen guard
    return _chatMenu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool MessagesApplet::onInput(InputEvent ev) {
  // Long-press chat-action overlay swallows input until it closes.
  if (_menuOpen) {
    if (_chatMenu.confirming()) {   // destructive action awaiting Confirm/Cancel
      _chatMenu.onInput(ev);
      const char* toast = nullptr;
      ChatMenu::Result r = _chatMenu.takeResult(toast);
      if (_host && toast) _host->postToast(toast);
      if (r != ChatMenu::Result::None) { syncList(); _menuOpen = false; }   // confirmed -> done
      return true;   // cancel just dismisses the dialog, back to the action list
    }
    if (_chatMenu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      const char* toast = nullptr;
      ChatMenu::Result r = _chatMenu.activate(_svc, toast);   // Mark unread runs now; Clear/Delete arm confirm
      if (_host && toast) _host->postToast(toast);
      if (r != ChatMenu::Result::None) syncList();   // chat cleared/removed -> refresh rows
      if (!_chatMenu.confirming()) _menuOpen = false;   // non-destructive (or none) closes the overlay
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _menuOpen = false; return true; }
    return true;
  }

  if (_tabs.onInput(ev)) {
    _tab = _tabs.selected();
    syncList();
    return true;
  }
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    if (_tab == 0) {
      ConvoView v;
      if (_svc && _svc->getConvo(_list.selected(), v)) {
        messageThreadApplet().setTarget(v.key);
        if (_host) _host->push(&messageThreadApplet());
      }
    } else {
      if (_list.selected() == 0) {                 // "New message" -> contact picker
        contactsApplet().beginPick();
        if (_host) _host->push(&contactsApplet());
      } else if (_host) {
        _host->postToast("Not available yet");     // "New group"
      }
    }
    return true;
  }
  if (ev == InputEvent::SelectLong && _tab == 0) {
    ConvoView v;
    if (_svc && _svc->getConvo(_list.selected(), v)) {
      _chatMenu.setTarget(v.key);
      _chatMenu.reset();
      _menuOpen = true;
    }
    return true;
  }
  return false;   // Back bubbles -> pop to app menu
}

MessagesApplet& messagesApplet() {
  static MessagesApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&messagesApplet(), Placement::AppMenu, "Messages", 1,
                              (uint16_t)Icon::Message);

}  // namespace mishmesh
