// mishmesh/applets/MessageThreadApplet.cpp
#include "MessageThreadApplet.h"
#include "MessagePathApplet.h"
#include "ChatNotifyApplet.h"
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/StatusBar.h>   // batteryPercent()
#include <mishmesh/widgets/Modal.h>
#include <cstdio>
#include <cstring>

namespace mishmesh {

static const int HDR_H   = 13;  // status bar height
static const int GUTTER  = 7;   // left gutter on inbound messages (focus caret + indent)
static const int PAD     = 2;   // bubble inner padding
static const int GAP     = 3;   // vertical gap between message blocks
static const int SB_GUTTER = 4; // width reserved on the right when a scrollbar shows
static const int BTN_H   = 36;  // 2-button action bar at the bottom of the thread

// Word-wraps `text` to `maxW` using fontBody. When draw is true, renders each
// line into `c` starting at (x,y); otherwise only measures. Returns total height
// in px. No allocation: lines are assembled in a fixed stack buffer.
static int wrapText(Canvas& c, int x, int y, int maxW, const char* text,
                    DisplayDriver::Color col, bool draw) {
  const mf_font_s* f = fontBody();
  int adv = c.lineHeight(f);
  if (adv <= 0) adv = 8;
  char line[80];
  int  lineLen = 0;
  int  lines = 0;
  const char* p = text ? text : "";
  while (*p) {
    const char* w0 = p;
    while (*p && *p != ' ' && *p != '\n') p++;
    int wlen = (int)(p - w0);
    if (wlen > (int)sizeof(line) - 1) wlen = (int)sizeof(line) - 1;   // clamp a word to the line buffer

    char cand[80];
    int  clen = 0;
    if (lineLen > 0) {
      memcpy(cand, line, lineLen); clen = lineLen;
      if (clen < (int)sizeof(cand) - 1) cand[clen++] = ' ';
    }
    int room = (int)sizeof(cand) - 1 - clen;        // bytes left for the word (never negative below)
    int copy = wlen < room ? wlen : room;
    if (copy < 0) copy = 0;
    memcpy(cand + clen, w0, copy); clen += copy; cand[clen] = 0;

    if (lineLen > 0 && c.textWidth(f, cand) > maxW) {
      if (draw) { line[lineLen] = 0; c.drawText(f, x, y + lines * adv, line, col); }
      lines++;
      int wl = wlen;
      if (wl > (int)sizeof(line) - 1) wl = (int)sizeof(line) - 1;
      memcpy(line, w0, wl); lineLen = wl; line[lineLen] = 0;
    } else {
      memcpy(line, cand, clen); lineLen = clen; line[lineLen] = 0;
    }
    while (*p == ' ') p++;
    if (*p == '\n') {                       // hard break
      if (draw) { line[lineLen] = 0; c.drawText(f, x, y + lines * adv, line, col); }
      lines++; lineLen = 0; p++;
    }
  }
  if (lineLen > 0) { if (draw) { line[lineLen] = 0; c.drawText(f, x, y + lines * adv, line, col); } lines++; }
  return lines * adv;
}

// Builds the metadata string shown below an outbound bubble.
static void outboundStatus(const MessageView& m, char* buf, int cap) {
  if (m.isChannel) {
    if (m.heardCount) snprintf(buf, cap, "Heard %u", (unsigned)m.heardCount);
    else              snprintf(buf, cap, "Sent");
  } else if (m.status == ST_DELIVERED) {
    snprintf(buf, cap, "Delivered %.1fs", m.tripTimeMs / 1000.0);
  } else if (m.status == ST_FAILED) {
    snprintf(buf, cap, "Failed");
  } else {
    snprintf(buf, cap, "Sent");
  }
}

void MessageThreadApplet::setTarget(const ConvoKey& k, const char* fallbackName) {
  _key = k;
  if (fallbackName) { strncpy(_fallbackName, fallbackName, sizeof(_fallbackName) - 1);
                      _fallbackName[sizeof(_fallbackName) - 1] = 0; }
  else _fallbackName[0] = 0;
}

void MessageThreadApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.messages; _app = ctx.app;
  int n = _svc ? _svc->messageCount(_key) : 0;
  _focus = n > 0 ? n - 1 : 0;
  _scrollY = _scrollTarget = 0;
  _animReady = false;   // snap (no slide-in) on the opening frame
  _pinBottom = true;    // first render scrolls to the newest message
  _lastSeq = _svc ? _svc->seq() : 0;
  _prevCount = n;
  _lastMsgTime = 0;        // seed from the newest message so the open frame isn't seen as an arrival
  if (n > 0 && _svc) {
    MessageView mv;
    if (_svc->getMessage(_key, n - 1, mv)) _lastMsgTime = mv.localTime ? mv.localTime : mv.senderTime;
  }
  _unseenBelow = false;    // we open pinned to the newest message
  _menuOpen = false;
  _barRow = _composeOnOpen ? 0 : -1;   // open focused on the Write button when requested
  _composeOnOpen = false;              // one-shot

  // Header tab bar: [0] conversation (label tracks _titleBuf), [1] Settings.
  snprintf(_titleBuf, sizeof(_titleBuf), "%s", resolveTitle());
  _tabs.clear();
  _tabs.addTab(_titleBuf, (uint16_t)(_key.type == 1 ? Icon::Users : Icon::User));
  _tabs.addTab("Settings", (uint16_t)Icon::Settings);
  _tabs.setSelected(0);
  _chatMenu.setTarget(_key);
  _chatMenu.reset();
  refreshRegion();

  if (_svc) _svc->setActiveConvo(_key);
}

void MessageThreadApplet::onForeground() {
  snprintf(_titleBuf, sizeof(_titleBuf), "%s", resolveTitle());
  if (_svc) _svc->setActiveConvo(_key);
  refreshRegion();   // re-sync the menu's Region/Notifications values on return from an editor
}
void MessageThreadApplet::onBackground() { if (_svc) _svc->clearActiveConvo(); }
void MessageThreadApplet::onStop() {
  if (!_svc) return;
  // Leaving the chat with a message still below the fold (chevron showing) means it
  // was never actually read - being the active convo had suppressed its unread
  // badge, so restore one on the way out.
  if (_unseenBelow) _svc->markUnread(_key);
  _svc->clearActiveConvo();
}

const char* MessageThreadApplet::resolveTitle() const {
  ConvoView cv;
  for (int i = 0; _svc && i < _svc->convoCount(); i++) {
    if (_svc->getConvo(i, cv) && cv.key.equals(_key)) return cv.name;
  }
  return _fallbackName;
}

int MessageThreadApplet::blockHeight(Canvas& body, const MessageView& m) const {
  int adv = body.lineHeight(fontBody());    if (adv <= 0) adv = 8;
  int cap = body.lineHeight(fontCaption()); if (cap <= 0) cap = 6;   // recessive metadata row
  int cw = _contentW;
  if (m.outbound) {
    int bubbleW = (cw * 3) / 4;
    int bodyH = wrapText(body, 0, 0, bubbleW - 2 * PAD, m.text, DisplayDriver::DARK, false);
    if (bodyH < adv) bodyH = adv;
    return (bodyH + 2 * PAD) + cap + GAP;          // bubble (body font) + caption status + gap
  }
  int bodyH = wrapText(body, 0, 0, cw - GUTTER - PAD, m.text, DisplayDriver::LIGHT, false);
  if (bodyH < adv) bodyH = adv;
  return cap + bodyH + GAP;                         // caption sender/divider row + body + gap
}

void MessageThreadApplet::layoutFocus(Canvas& body, int n) {
  int top = 0;
  _focusTop = _focusBot = 0;
  for (int i = 0; i < n; i++) {
    MessageView m; if (!_svc->getMessage(_key, i, m)) continue;
    int bh = blockHeight(body, m);
    if (i == _focus && _barRow < 0) { _focusTop = top; _focusBot = top + bh; }
    top += bh;
  }
  if (_barRow >= 0) { _focusTop = top; _focusBot = top + BTN_H; }  // whole bar visible
  _contentH = top + BTN_H;   // bar always present below the messages
}

// Pulls the committed scroll target just far enough to keep the focused message
// visible (the eased _scrollY then glides toward it).
void MessageThreadApplet::adjustScroll() {
  int vp = _bodyH;
  if (_contentH <= vp) { _scrollTarget = 0; return; }
  if (_focusBot - _focusTop >= vp) {               // focused message taller than viewport
    if (_scrollTarget < _focusTop)      _scrollTarget = _focusTop;
    if (_scrollTarget > _focusBot - vp) _scrollTarget = _focusBot - vp;
  } else {
    if (_focusTop < _scrollTarget)       _scrollTarget = _focusTop;
    if (_focusBot > _scrollTarget + vp)  _scrollTarget = _focusBot - vp;
  }
  if (_scrollTarget < 0) _scrollTarget = 0;
  if (_scrollTarget > _contentH - vp) _scrollTarget = _contentH - vp;
}

void MessageThreadApplet::drawMessage(Canvas& body, const MessageView& m, int top, bool focused) const {
  int adv = body.lineHeight(fontBody());    if (adv <= 0) adv = 8;
  int cap = body.lineHeight(fontCaption()); if (cap <= 0) cap = 6;
  int bh = blockHeight(body, m);
  // Clip the whole block to its own region (negative offset keeps mapping).
  Canvas blk = body.region(0, top - _scrollY, body.width(), bh);
  int cw = _contentW;

  if (m.outbound) {
    int bubbleW = (cw * 3) / 4;
    int bx = cw - bubbleW;
    int bodyH = wrapText(blk, 0, 0, bubbleW - 2 * PAD, m.text, DisplayDriver::DARK, false);
    if (bodyH < adv) bodyH = adv;
    int bubbleH = bodyH + 2 * PAD;
    blk.fillRect(bx, 0, bubbleW, bubbleH, DisplayDriver::LIGHT);
    wrapText(blk, bx + PAD, PAD, bubbleW - 2 * PAD, m.text, DisplayDriver::DARK, true);
    char st[28]; outboundStatus(m, st, sizeof(st));
    blk.drawText(fontCaption(), cw, bubbleH, st, DisplayDriver::LIGHT, TextAlign::Right);
    if (focused) blk.drawText(fontBody(), bx - 6, (bubbleH - adv) / 2, ">", DisplayDriver::LIGHT);
    return;
  }

  // Inbound: no box. A caption-size sender label (channel) or a short divider marks the start.
  if (m.isChannel && m.senderName && m.senderName[0]) {
    blk.drawText(fontCaption(), GUTTER, 0, m.senderName, DisplayDriver::LIGHT);
    int nw = blk.textWidth(fontCaption(), m.senderName);
    blk.fillRect(GUTTER, cap - 1, nw, 1, DisplayDriver::LIGHT);   // underline = it's a label
  } else {
    blk.fillRect(GUTTER, cap / 2, 16, 1, DisplayDriver::LIGHT);   // short divider
  }
  wrapText(blk, GUTTER, cap, cw - GUTTER - PAD, m.text, DisplayDriver::LIGHT, true);
  if (focused) blk.drawText(fontBody(), 0, cap, ">", DisplayDriver::LIGHT);
}

// Draws the two stacked Write/Quick buttons into a BTN_H-tall region; the
// focused button (per _barRow) renders filled.
void MessageThreadApplet::drawActionBar(Canvas& bar) {
  const int MX = 2, MY = 1, GAP = 2;             // outer margin + inter-button gap
  int bw = bar.width() - 2 * MX;
  int bh = (BTN_H - 2 * MY - GAP) / 2;
  _btn.set("Write a message", (uint16_t)Icon::Feather);
  _btn.setFocused(_barRow == 0);
  _btn.draw(bar, MX, MY, bw, bh);
  _btn.set("Quick replies", (uint16_t)Icon::Zap);
  _btn.setFocused(_barRow == 1);
  _btn.draw(bar, MX, MY + bh + GAP, bw, bh);
}

int MessageThreadApplet::onRender(Canvas& c) {
  int n = _svc ? _svc->messageCount(_key) : 0;

  // Detect a genuinely new message by the newest message's timestamp, not by count.
  // At the per-chat cap an arrival evicts the oldest, so messageCount() stays flat
  // and a count test would miss it entirely. localTime is our receive clock; fall
  // back to senderTime when the RTC is unset. The count test is kept as a secondary
  // signal for the (common) below-cap case so same-second arrivals still register.
  uint32_t newestTime = 0;
  if (n > 0 && _svc) {
    MessageView mv;
    if (_svc->getMessage(_key, n - 1, mv)) newestTime = mv.localTime ? mv.localTime : mv.senderTime;
  }
  bool newArrived = (n > _prevCount) || (newestTime > _lastMsgTime);

  if (newArrived) {
    // The caret never follows an arrival - the user's reading position stays put.
    // But if the cap evicted the oldest to make room, every index shifted down, so
    // step the caret down by however many were dropped to keep it on the SAME
    // message instead of letting it jump to the next one. (Assumes one new message
    // per step, which holds in practice - the UI repaints between arrivals.)
    int evicted = (_prevCount + 1) - n;
    if (evicted > 0) { _focus -= evicted; if (_focus < 0) _focus = 0; }
  }
  _prevCount = n;
  _lastMsgTime = newestTime;
  if (_svc && _svc->seq() != _lastSeq) {
    _lastSeq = _svc->seq();
    if (_focus >= n) _focus = n > 0 ? n - 1 : 0;   // clamp after deletions
  }

  // Header: tab bar ([0] conversation, [1] Settings) with a floating battery %.
  snprintf(_battBuf, sizeof(_battBuf), "%d%%", batteryPercent(_app ? _app->batteryMillivolts() : 0));
  _tabs.setDecoration(_battBuf);
  _tabs.draw(c, 0, 0, c.width(), HDR_H);

  Canvas body = c.region(0, HDR_H, c.width(), c.height() - HDR_H);
  _bodyH = body.height();

  // Settings tab: the shared chat menu inline (replaces the message view).
  if (_tabs.selected() == 1) {
    body.fillRect(0, 0, body.width(), body.height(), DisplayDriver::DARK);
    _chatMenu.draw(body, 4, 2, body.width() - 8, body.height() - 4);
    if (_chatMenu.confirming())   // full-screen guard over header + menu
      _chatMenu.drawConfirm(c, 0, 0, c.width(), c.height());
    return _chatMenu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }

  // Empty thread: the hint centered in the space above a bottom-pinned action
  // bar (no scroll machinery - there's nothing to lay out).
  if (n == 0) {
    int barTop = body.height() - BTN_H; if (barTop < 0) barTop = 0;
    int hintY = (barTop - body.fontHeight(fontBody())) / 2; if (hintY < 0) hintY = 0;
    body.drawText(fontBody(), body.width() / 2, hintY,
                  "No messages", DisplayDriver::LIGHT, TextAlign::Center);
    Canvas barC = body.region(0, barTop, body.width(), BTN_H);
    drawActionBar(barC);
    return 500;
  }

  // Lay out at full width; only if the thread overflows do we reserve a right
  // gutter for the scrollbar (and re-lay out), matching the list/text views.
  _contentW = body.width();
  layoutFocus(body, n);
  bool scrollbar = _contentH > _bodyH;
  if (scrollbar) { _contentW = body.width() - SB_GUTTER; layoutFocus(body, n); }

  if (_pinBottom) {
    _scrollTarget = (_contentH > _bodyH) ? _contentH - _bodyH : 0;
    _animReady = false;          // snap to the bottom on open, no slide-in
    _pinBottom = false;
  }
  adjustScroll();

  // New-message chevron, decided from actual visibility (the caret is never moved).
  // The newest message's bottom sits BTN_H above the content end - the action bar
  // always trails it - so test against that, not the absolute bottom. Raise the
  // marker only when a message *arrived* below the fold; clear it the moment the
  // newest message is back in view. Plain scrolling through history never raises it.
  {
    int newestBottom = _contentH - BTN_H;
    bool newestVisible = (n == 0) || (newestBottom <= _scrollTarget + _bodyH);
    if (newArrived && !newestVisible) _unseenBelow = true;
    if (newestVisible)                _unseenBelow = false;
  }

  // Ease the current scroll toward the target (same feel as ListMenu/ScrollText).
  if (!_animReady) { _scrollY = _scrollTarget; _animReady = true; }
  else {
    int step = _bodyH / 3; if (step < 4) step = 4;
    _scrollY = approach(_scrollY, _scrollTarget, step);
  }
  bool animating = (_scrollY != _scrollTarget);

  int top = 0;
  for (int i = 0; i < n; i++) {
    MessageView m; if (!_svc->getMessage(_key, i, m)) continue;
    int bh = blockHeight(body, m);
    int y = top - _scrollY;
    if (y + bh > 0 && y < _bodyH) drawMessage(body, m, top, i == _focus);
    top += bh;
    if (y >= _bodyH) break;
  }

  // Action bar: two stacked bordered buttons as an inline trailing block below
  // the messages. Drawn here (this applet owns the _barRow focus state machine).
  {
    int by = (_contentH - BTN_H) - _scrollY;
    if (by + BTN_H > 0 && by < _bodyH) {
      Canvas barC = body.region(0, by, _contentW, BTN_H);
      drawActionBar(barC);
    }
  }

  // Scrollbar thumb on the right edge (content already cleared SB_GUTTER for it).
  if (scrollbar) {
    int thumbH = _bodyH * _bodyH / _contentH; if (thumbH < 3) thumbH = 3;
    int maxScroll = _contentH - _bodyH;
    int thumbY = maxScroll > 0 ? (_bodyH - thumbH) * _scrollY / maxScroll : 0;
    body.fillRect(body.width() - 2, thumbY, 2, thumbH, DisplayDriver::LIGHT);
  }

  // New-message chevron: a small downward marker pinned to the bottom-centre of
  // the viewport while there's unread content below the fold. Slow blink to draw
  // the eye; clears itself (above) once the user scrolls back down.
  if (_unseenBelow) {
    const int bw = 13, bh = 8;
    int bx = (body.width() - bw) / 2, by = _bodyH - bh;
    body.fillRect(bx, by, bw, bh, DisplayDriver::DARK);
    body.drawRoundRect(bx, by, bw, bh, DisplayDriver::LIGHT);
    if (((body.now() / 500) & 1) == 0) {           // blink at ~1 Hz
      int cx = bx + bw / 2, ty = by + 2;
      body.fillRect(cx - 2, ty,     5, 1, DisplayDriver::LIGHT);
      body.fillRect(cx - 1, ty + 1, 3, 1, DisplayDriver::LIGHT);
      body.fillRect(cx,     ty + 2, 1, 1, DisplayDriver::LIGHT);
    }
  }

  // Per-message action menu: a bare box over the live conversation.
  if (_menuOpen) {
    Canvas box = drawModalChrome(c);
    _menu.draw(box, 2, 2, box.width() - 4, box.height() - 4);
    return _menu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }
  return animating ? ListMenu::TICK_MS : 500;
}

bool MessageThreadApplet::onInput(InputEvent ev) {
  // The per-message overlay swallows everything until it closes.
  if (_menuOpen) {
    if (_menu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      int sel = _menu.selected();
      if (sel == 1) {
        if (_svc) _svc->deleteMessage(_key, _focus);
        _menuOpen = false;
      } else if (sel == 2 && _msgMenu.hasPath) {
        _menuOpen = false;
        messagePathApplet().setTarget(_key, _focus);
        if (_host) _host->push(&messagePathApplet());
      } else if (sel == 0) {                 // Reply -> compose, seeding @[sender] for channels
        _menuOpen = false;
        char seed[48] = {0};
        MessageView m;
        if (_key.type == 1 && _svc && _svc->getMessage(_key, _focus, m) &&
            m.senderName && m.senderName[0])
          snprintf(seed, sizeof(seed), "@[%s] ", m.senderName);   // app's mention format
        startCompose(seed);
      } else {
        _menuOpen = false;
        if (_host) _host->postToast("Not available yet");
      }
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _menuOpen = false; return true; }
    return true;   // swallow all input while the menu is open
  }

  // The chat menu's confirm dialog is a modal: once armed it captures all input
  // ahead of the tab bar, same modal-first rule every screen follows (cf.
  // ContactsApplet guarding its confirm before its tabs). Without this the tab
  // bar below would eat NavLeft/NavRight before the dialog's buttons saw them.
  if (_chatMenu.confirming()) {
    _chatMenu.onInput(ev);
    const char* toast = nullptr;
    ChatMenu::Result r = _chatMenu.takeResult(toast);
    if (_host && toast) _host->postToast(toast);
    if (r == ChatMenu::Result::Deleted) { if (_host) _host->pop(); }    // chat is gone
    else if (r == ChatMenu::Result::Cleared) { _tabs.setSelected(0); _pinBottom = true; }
    return true;   // cancel just dismisses the dialog, back to the action list
  }

  // NavLeft/NavRight switch tabs (unused by the conversation view otherwise).
  if (_tabs.onInput(ev)) return true;

  // Settings tab: drive the shared chat menu.
  if (_tabs.selected() == 1) {
    if (_chatMenu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      const char* toast = nullptr;
      ChatMenu::Result r = _chatMenu.activate(_svc, toast);   // Mark unread runs now; Clear/Delete arm confirm
      if (r == ChatMenu::Result::EditRegion) { openRegionEditor(); return true; }   // keypad over the menu
      if (r == ChatMenu::Result::EditNotify) {
        chatNotifyApplet().setTarget(_key, _titleBuf);
        if (_host) _host->push(&chatNotifyApplet());
        return true;
      }
      if (_host && toast) _host->postToast(toast);
      if (r == ChatMenu::Result::Deleted) { if (_host) _host->pop(); }   // chat is gone
      else if (!_chatMenu.confirming()) { _tabs.setSelected(0); _pinBottom = true; }  // non-destructive -> conversation
      return true;
    }
    return false;   // Back bubbles -> pop the thread
  }

  return onConversationInput(ev);
}

bool MessageThreadApplet::onConversationInput(InputEvent ev) {
  int n = _svc ? _svc->messageCount(_key) : 0;

  // Empty thread: the action bar is the only focusable region, and the n==0
  // render path never computes the message-layout fields the scroll logic below
  // relies on (which are stale from the previously-viewed thread - this applet is
  // a reused singleton). Keep focus inside the bar so Write stays reachable.
  if (n == 0) {
    if (ev == InputEvent::NavDown) { _barRow = (_barRow < 1) ? _barRow + 1 : 1; return true; }
    if (ev == InputEvent::NavUp)   { _barRow = (_barRow > 0) ? _barRow - 1 : 0; return true; }
    if (ev == InputEvent::Select) {
      if (_barRow <= 0) startCompose();
      else if (_host) _host->postToast("Not available yet");
      return true;
    }
    return false;   // Back bubbles -> pop to chat list (Left/Right consumed by tabs)
  }

  // Nav decisions use the committed target (not the eased value) to avoid chatter.
  int vp = _bodyH > 0 ? _bodyH : 48;
  int step = vp > 16 ? (vp * 2) / 3 : 12;

  // Button bar has focus: vertical nav moves between/out of the two buttons;
  // Left/Right and Back are left unconsumed so they bubble (tabs / pop).
  if (_barRow >= 0) {
    if (ev == InputEvent::NavDown) { if (_barRow < 1) _barRow++; return true; }
    if (ev == InputEvent::NavUp)   { _barRow = (_barRow > 0) ? _barRow - 1 : -1; return true; }
    if (ev == InputEvent::Select) {
      if (_barRow == 0) startCompose();
      else if (_host) _host->postToast("Not available yet");
      return true;
    }
    return false;
  }

  if (ev == InputEvent::NavUp) {
    if (_focusTop < _scrollTarget) {             // more of the focused message is above
      _scrollTarget -= step; if (_scrollTarget < _focusTop) _scrollTarget = _focusTop;
    } else if (_focus > 0) {
      _focus--;
    }
    return true;
  }
  if (ev == InputEvent::NavDown) {
    if (_focusBot > _scrollTarget + vp) {        // more of the focused message is below
      _scrollTarget += step; if (_scrollTarget > _focusBot - vp) _scrollTarget = _focusBot - vp;
    } else if (_focus < n - 1) {
      _focus++;
    } else {
      _barRow = 0;                               // past the last message -> action bar
    }
    return true;
  }
  if (ev == InputEvent::Select && n > 0) {
    MessageView m; bool hasPath = false, repeats = false;
    if (_svc && _svc->getMessage(_key, _focus, m)) {
      hasPath = (m.kind == KIND_INBOUND) || (m.kind == KIND_OUT_CHAN);
      repeats = (m.kind == KIND_OUT_CHAN);
    }
    _msgMenu.hasPath = hasPath;
    _msgMenu.repeats = repeats;
    _menu.setModel(&_msgMenu); _menu.setRowHeight(14); _menu.resetSelection();
    _menuOpen = true;
    return true;
  }
  return false;   // Back bubbles -> pop to chat list
}

void MessageThreadApplet::startCompose(const char* seed) {
  if (seed && seed[0]) { strncpy(_composeBuf, seed, KeypadApplet::KP_MAX);
                         _composeBuf[KeypadApplet::KP_MAX] = 0; }
  else _composeBuf[0] = 0;
  keypadApplet().configure(_composeBuf, KeypadApplet::KP_MAX, "Message",
                           &MessageThreadApplet::onComposeDone, this);
  if (_host) _host->push(&keypadApplet());
}

void MessageThreadApplet::onComposeDone(void* ctx, const char* text) {
  auto* self = static_cast<MessageThreadApplet*>(ctx);
  if (!text || !text[0] || !self->_svc) return;          // empty -> send nothing
  if (!self->_svc->sendText(self->_key, text)) {
    if (self->_host) self->_host->postToast("Send failed");
  }
}

void MessageThreadApplet::refreshRegion() {
  _regionBuf[0] = 0;
  if (_svc) _svc->region(_key, _regionBuf, sizeof(_regionBuf));
  _chatMenu.setRegion(_regionBuf);   // shows "None" when empty
  if (_svc) _chatMenu.setNotifyLabel(mishmesh::notifyLevelShortLabel(_svc->notifyLevel(_key)));
}

void MessageThreadApplet::openRegionEditor() {
  _regionBuf[0] = 0;
  if (_svc) _svc->region(_key, _regionBuf, sizeof(_regionBuf));   // seed with current
  keypadApplet().configure(_regionBuf, sizeof(_regionBuf) - 1, "Region",
                           &MessageThreadApplet::onRegionDone, this);
  if (_host) _host->push(&keypadApplet());
}

void MessageThreadApplet::onRegionDone(void* ctx, const char* text) {
  auto* self = static_cast<MessageThreadApplet*>(ctx);
  if (!self->_svc) return;
  self->_svc->setRegion(self->_key, text);   // empty -> clears to "None"
  self->refreshRegion();
}

MessageThreadApplet& messageThreadApplet() {
  static MessageThreadApplet a;
  return a;
}

} // namespace mishmesh
