#include "MessagesApplet.h"
#include <mishmesh/applets/settings/MessagesSettingsPanel.h>
#include "MessageThreadApplet.h"
#include "ChatNotifyApplet.h"
#include "ChannelShareApplet.h"
#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/applets/JoinPrivateApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/widgets/Modal.h>
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

MessagesApplet::NewAction MessagesApplet::NewModel::actionAt(int i) const {
  if (i == 0) return NewAction::Message;
  if (i == 1) return NewAction::CreatePrivate;
  if (i == 2) return NewAction::JoinPrivate;
  bool showPublic = svc && !svc->publicChannelJoined();
  if (showPublic && i == 3) return NewAction::JoinPublic;
  return NewAction::JoinHashtag;   // i == 4 (public shown) or i == 3 (public hidden)
}

const char* MessagesApplet::NewModel::label(int i) const {
  switch (actionAt(i)) {
    case NewAction::Message:       return "New message";
    case NewAction::CreatePrivate: return "Create private channel";
    case NewAction::JoinPrivate:   return "Join private channel";
    case NewAction::JoinPublic:    return "Join public channel";
    default:                       return "Join hashtag channel";
  }
}

uint16_t MessagesApplet::NewModel::icon(int i) const {
  switch (actionAt(i)) {
    case NewAction::Message:       return (uint16_t)Icon::Message;
    case NewAction::CreatePrivate: return (uint16_t)Icon::Users;
    case NewAction::JoinPrivate:   return (uint16_t)Icon::Users;
    case NewAction::JoinPublic:    return (uint16_t)Icon::Comment;
    default:                       return (uint16_t)Icon::Comment;
  }
}

// ---- MessagesApplet ----

void MessagesApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc  = ctx.messages;
  _app  = ctx.app;
  _chats.svc = _svc;
  _new.svc = _svc;
  messagesSettings().begin(ctx);
  _tabs.clear();
  _tabs.addTab("Chats", (uint16_t)Icon::Message);
  _tabs.addTab("New", (uint16_t)Icon::Plus);
  _tabs.addTab("Settings", (uint16_t)Icon::Settings);
  _tab = 0;
  syncList();
}

void MessagesApplet::onForeground() {
  _chats.svc = _svc;
  _new.svc = _svc;
  syncList();
  refreshRegion();
  // Returning from a chat: the recency sort may have reordered the list, so the
  // stored row index would now land on a different chat. Reselect the chat we
  // opened by key so the highlight stays on it.
  if (_hasOpened && _tab == 0 && _svc) {
    ConvoView v;
    for (int i = 0; i < _svc->convoCount(); i++) {
      if (_svc->getConvo(i, v) && v.key.equals(_openedKey)) { _list.setSelected(i); break; }
    }
    _hasOpened = false;
  }
}

void MessagesApplet::syncList() {
  ListModel* m = _tab == 0 ? (ListModel*)&_chats
               : _tab == 1 ? (ListModel*)&_new
                           : nullptr;          // tab 2 = Settings: rendered by messagesSettings()
  _list.setModel(m);
  _list.setRowHeight(14);
  _list.setEmptyText(_tab == 0 ? "No messages yet" : "");
}

int MessagesApplet::visibleRowCountForTest() const {
  return _tab == 0 ? _chats.count() : _tab == 1 ? _new.count() : 3;
}

int MessagesApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int barH = 13;
  _tabs.setBattery(_app ? _app->batteryMillivolts() : 0);
  _tabs.draw(c, 0, 0, w, barH);
  int bodyY = barH + 1;
  int bodyH = h - bodyY;

  if (settingsTab()) {
    return messagesSettings().renderBody(c, 0, bodyY, w, bodyH);
  }

  _list.draw(c, 0, bodyY, w, bodyH);

  if (_menuOpen) {
    Canvas box = drawModalChrome(c);   // bare box over the live list
    _chatMenu.draw(box, 2, 2, box.width() - 4, box.height() - 4);
    if (_chatMenu.modalActive()) _chatMenu.drawModal(c, 0, 0, w, h);   // full-screen guard
    return _chatMenu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool MessagesApplet::onInput(InputEvent ev) {
  // Settings tab: delegate entirely to the shared panel.
  if (settingsTab()) {
    if (messagesSettings().modalActive()) return messagesSettings().onInput(ev);
    if (_tabs.onInput(ev)) { _tab = _tabs.selected(); syncList(); return true; }
    if (messagesSettings().onInput(ev)) return true;
    return false;   // Back bubbles
  }

  // Long-press chat-action overlay swallows input until it closes.
  if (_menuOpen) {
    if (_chatMenu.modalActive()) {   // confirm dialog or notify stepper awaiting input
      _chatMenu.onInput(ev);
      const char* toast = nullptr;
      ChatMenu::Result r = _chatMenu.takeResult(toast);
      if (_host && toast) _host->postToast(toast);
      if (r != ChatMenu::Result::None) { syncList(); _menuOpen = false; }   // confirmed -> done
      return true;   // cancel just dismisses, back to the action list
    }
    if (_chatMenu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      const char* toast = nullptr;
      ChatMenu::Result r = _chatMenu.activate(_svc, toast);   // Mark unread runs now; Clear/Delete arm a modal; Notify opens the notify applet
      if (r == ChatMenu::Result::EditRegion) { openRegionEditor(); return true; }   // keypad over the menu
      if (r == ChatMenu::Result::EditNotify) {
        const char* n = "";
        ConvoView cv;
        for (int i = 0; _svc && i < _svc->convoCount(); i++) {
          if (_svc->getConvo(i, cv) && cv.key.equals(_menuKey)) { n = cv.name; break; }
        }
        chatNotifyApplet().setTarget(_menuKey, n);
        if (_host) _host->push(&chatNotifyApplet());
        return true;
      }
      if (r == ChatMenu::Result::Share) {
        const char* n = "";
        ConvoView cv;
        for (int i = 0; _svc && i < _svc->convoCount(); i++) {
          if (_svc->getConvo(i, cv) && cv.key.equals(_menuKey)) { n = cv.name; break; }
        }
        channelShareApplet().setTarget(_menuKey, n);
        _menuOpen = false;                 // returning from Share lands back on the list
        if (_host) _host->push(&channelShareApplet());
        return true;
      }
      if (_host && toast) _host->postToast(toast);
      if (r != ChatMenu::Result::None) syncList();   // chat cleared/removed -> refresh rows
      if (!_chatMenu.modalActive()) _menuOpen = false;   // non-destructive (or none) closes the overlay
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
        _openedKey = v.key; _hasOpened = true;   // reselect this chat by key on return
        messageThreadApplet().setTarget(v.key);
        if (_host) _host->push(&messageThreadApplet());
      }
    } else {
      switch (_new.actionAt(_list.selected())) {
        case NewAction::Message:
          contactsApplet().beginPick();
          if (_host) _host->push(&contactsApplet());
          break;
        case NewAction::CreatePrivate: openCreatePrivate(); break;
        case NewAction::JoinPrivate:   openJoinPrivate();   break;
        case NewAction::JoinPublic:
          applyResult(_svc ? _svc->joinPublicChannel() : ChanResult::Error, "Joined Public");
          break;
        case NewAction::JoinHashtag:   openJoinHashtag();   break;
      }
    }
    return true;
  }
  if (ev == InputEvent::SelectLong && _tab == 0) {
    ConvoView v;
    if (_svc && _svc->getConvo(_list.selected(), v)) {
      _menuKey = v.key;
      _chatMenu.setTarget(v.key);
      _chatMenu.reset();
      refreshRegion();
      _menuOpen = true;
    }
    return true;
  }
  return false;   // Back bubbles -> pop to app menu
}

bool MessagesApplet::isHexKey(const char* s) {
  int n = 0;
  for (const char* p = s; *p; ++p, ++n) {
    char c = *p;
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!hex) return false;
  }
  return n == 32;
}

bool MessagesApplet::applyResult(ChanResult res, const char* okToast) {
  const char* t = "Failed"; bool pop = false;
  switch (res) {
    case ChanResult::Ok:        t = okToast; pop = true;
                                _tabs.setSelected(0); _tab = 0; syncList(); break;
    case ChanResult::Full:      t = "Channels full"; break;
    case ChanResult::Invalid:   t = "Invalid key"; break;
    case ChanResult::Duplicate: t = "Already joined"; pop = true; break;
    case ChanResult::Error:     default: t = "Failed"; break;
  }
  if (_host) _host->postToast(t);
  return pop;
}

void MessagesApplet::openCreatePrivate() {
  _chName[0] = 0;
  keypadApplet().configure(_chName, sizeof(_chName) - 1, "Create private",
                           &MessagesApplet::onCreatePrivateDone, this);
  if (_host) _host->push(&keypadApplet());
}

void MessagesApplet::openJoinPrivate() {
  _chName[0] = 0; _chKey[0] = 0;
  joinPrivateApplet().configure(_chName, sizeof(_chName), _chKey, sizeof(_chKey),
                                &MessagesApplet::isHexKey, &MessagesApplet::submitJoinPrivate, this);
  if (_host) _host->push(&joinPrivateApplet());
}

void MessagesApplet::openJoinHashtag() {
  _chName[0] = 0;
  keypadApplet().configure(_chName, sizeof(_chName) - 1, "Join hashtag",
                           &MessagesApplet::onJoinHashtagDone, this);
  if (_host) _host->push(&keypadApplet());
}

void MessagesApplet::refreshRegion() {
  _regionBuf[0] = 0;
  if (_svc) _svc->region(_menuKey, _regionBuf, sizeof(_regionBuf));
  _chatMenu.setRegion(_regionBuf);   // shows "None" when empty
  if (_svc) _chatMenu.setNotifyLabel(mishmesh::notifyLevelShortLabel(_svc->notifyLevel(_menuKey)));
}

void MessagesApplet::openRegionEditor() {
  _regionBuf[0] = 0;
  if (_svc) _svc->region(_menuKey, _regionBuf, sizeof(_regionBuf));   // seed with current
  keypadApplet().configure(_regionBuf, sizeof(_regionBuf) - 1, "Region",
                           &MessagesApplet::onRegionDone, this);
  if (_host) _host->push(&keypadApplet());
}

void MessagesApplet::onRegionDone(void* ctx, const char* text) {
  auto* a = static_cast<MessagesApplet*>(ctx);
  if (!a->_svc) return;
  a->_svc->setRegion(a->_menuKey, text);   // empty -> clears to "None"
  a->refreshRegion();
}

void MessagesApplet::onCreatePrivateDone(void* ctx, const char* text) {
  MessagesApplet* a = (MessagesApplet*)ctx;
  if (!text || !text[0]) { if (a->_host) a->_host->postToast("Name required"); return; }
  ChanResult r = a->_svc ? a->_svc->createPrivateChannel(text) : ChanResult::Error;
  char ok[40]; snprintf(ok, sizeof(ok), "Created %s", text);
  a->applyResult(r, ok);   // toast + (on Ok) switch to Chats; keypad already pops itself
}

bool MessagesApplet::submitJoinPrivate(void* ctx) {
  MessagesApplet* a = (MessagesApplet*)ctx;
  ChanResult r = a->_svc ? a->_svc->joinPrivateChannel(a->_chName, a->_chKey) : ChanResult::Error;
  char ok[40]; snprintf(ok, sizeof(ok), "Joined %s", a->_chName);
  return a->applyResult(r, ok);
}

void MessagesApplet::onJoinHashtagDone(void* ctx, const char* text) {
  MessagesApplet* a = (MessagesApplet*)ctx;
  if (!text || !text[0]) { if (a->_host) a->_host->postToast("Hashtag required"); return; }
  ChanResult r = a->_svc ? a->_svc->joinHashtagChannel(text) : ChanResult::Error;
  a->applyResult(r, "Joined channel");
}

void MessagesApplet::setChannelNameForTest(const char* s) {
  snprintf(_chName, sizeof(_chName), "%s", s ? s : "");
}
void MessagesApplet::setChannelKeyForTest(const char* s) {
  snprintf(_chKey, sizeof(_chKey), "%s", s ? s : "");
}

MessagesApplet& messagesApplet() {
  static MessagesApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&messagesApplet(), Placement::AppMenu, "Messages", 0,
                              (uint16_t)Icon::Message);

}  // namespace mishmesh
