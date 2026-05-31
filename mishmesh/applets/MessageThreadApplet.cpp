// mishmesh/applets/MessageThreadApplet.cpp
#include "MessageThreadApplet.h"
#include "MessagePathApplet.h"
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/text/Fonts.h>
#include <cstdio>
#include <cstring>

namespace mishmesh {

static const int HDR_H   = 13;  // status bar height
static const int GUTTER  = 7;   // left gutter on inbound messages (focus caret + indent)
static const int PAD     = 2;   // bubble inner padding
static const int GAP     = 3;   // vertical gap between message blocks
static const int SB_GUTTER = 4; // width reserved on the right when a scrollbar shows

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

    char cand[80];
    int  clen = 0;
    if (lineLen > 0) { memcpy(cand, line, lineLen); clen = lineLen; cand[clen++] = ' '; }
    int copy = wlen;
    if (copy > (int)sizeof(cand) - clen - 1) copy = (int)sizeof(cand) - clen - 1;
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

void MessageThreadApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.messages; _app = ctx.app;
  int n = _svc ? _svc->messageCount(_key) : 0;
  _focus = n > 0 ? n - 1 : 0;
  _scrollY = _scrollTarget = 0;
  _animReady = false;   // snap (no slide-in) on the opening frame
  _pinBottom = true;    // first render scrolls to the newest message
  _lastSeq = _svc ? _svc->seq() : 0;
  if (_svc) _svc->setActiveConvo(_key);
}

void MessageThreadApplet::onForeground() { if (_svc) _svc->setActiveConvo(_key); }
void MessageThreadApplet::onBackground() { if (_svc) _svc->clearActiveConvo(); }
void MessageThreadApplet::onStop()       { if (_svc) _svc->clearActiveConvo(); }

const char* MessageThreadApplet::resolveTitle() const {
  ConvoView cv;
  for (int i = 0; _svc && i < _svc->convoCount(); i++) {
    if (_svc->getConvo(i, cv) && cv.key.equals(_key)) return cv.name;
  }
  return "";
}

int MessageThreadApplet::blockHeight(Canvas& body, const MessageView& m) const {
  int adv = body.lineHeight(fontBody());
  if (adv <= 0) adv = 8;
  int cw = _contentW;
  if (m.outbound) {
    int bubbleW = (cw * 3) / 4;
    int bodyH = wrapText(body, 0, 0, bubbleW - 2 * PAD, m.text, DisplayDriver::DARK, false);
    if (bodyH < adv) bodyH = adv;
    return (bodyH + 2 * PAD) + adv + GAP;          // bubble + status line + gap
  }
  int sepH = adv;                                  // sender label / divider row
  int bodyH = wrapText(body, 0, 0, cw - GUTTER - PAD, m.text, DisplayDriver::LIGHT, false);
  if (bodyH < adv) bodyH = adv;
  return sepH + bodyH + GAP;
}

void MessageThreadApplet::layoutFocus(Canvas& body, int n) {
  int top = 0;
  _focusTop = _focusBot = 0;
  for (int i = 0; i < n; i++) {
    MessageView m; if (!_svc->getMessage(_key, i, m)) continue;
    int bh = blockHeight(body, m);
    if (i == _focus) { _focusTop = top; _focusBot = top + bh; }
    top += bh;
  }
  _contentH = top;
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
  int adv = body.lineHeight(fontBody());
  if (adv <= 0) adv = 8;
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
    blk.drawText(fontBody(), cw, bubbleH, st, DisplayDriver::LIGHT, TextAlign::Right);
    if (focused) blk.drawText(fontBody(), bx - 6, (bubbleH - adv) / 2, ">", DisplayDriver::LIGHT);
    return;
  }

  // Inbound: no box. A sender label (channel) or a short divider marks the start.
  if (m.isChannel && m.senderName && m.senderName[0]) {
    blk.drawText(fontBody(), GUTTER, 0, m.senderName, DisplayDriver::LIGHT);
    int nw = blk.textWidth(fontBody(), m.senderName);
    blk.fillRect(GUTTER, adv - 1, nw, 1, DisplayDriver::LIGHT);   // underline = it's a label
  } else {
    blk.fillRect(GUTTER, adv / 2, 16, 1, DisplayDriver::LIGHT);   // short divider
  }
  wrapText(blk, GUTTER, adv, cw - GUTTER - PAD, m.text, DisplayDriver::LIGHT, true);
  if (focused) blk.drawText(fontBody(), 0, adv, ">", DisplayDriver::LIGHT);
}

int MessageThreadApplet::onRender(Canvas& c) {
  int n = _svc ? _svc->messageCount(_key) : 0;
  if (_svc && _svc->seq() != _lastSeq) {
    _lastSeq = _svc->seq();
    if (_focus >= n) _focus = n > 0 ? n - 1 : 0;
  }

  // Header: shared small status bar (title + battery).
  _status.setTitle(resolveTitle());
  _status.setBattery(_app ? _app->batteryMillivolts() : 0);
  _status.draw(c, 0, 0, c.width(), HDR_H);

  Canvas body = c.region(0, HDR_H, c.width(), c.height() - HDR_H);
  _bodyH = body.height();

  if (_menuOpen) {
    body.fillRect(0, 0, body.width(), body.height(), DisplayDriver::DARK);   // opaque backdrop
    _menu.draw(body, 4, 2, body.width() - 8, body.height() - 4);
    return _menu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }

  if (n == 0) {
    body.drawText(fontBody(), body.width() / 2, body.height() / 2 - 4,
                  "No messages", DisplayDriver::LIGHT, TextAlign::Center);
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

  // Scrollbar thumb on the right edge (content already cleared SB_GUTTER for it).
  if (scrollbar) {
    int thumbH = _bodyH * _bodyH / _contentH; if (thumbH < 3) thumbH = 3;
    int maxScroll = _contentH - _bodyH;
    int thumbY = maxScroll > 0 ? (_bodyH - thumbH) * _scrollY / maxScroll : 0;
    body.fillRect(body.width() - 2, thumbY, 2, thumbH, DisplayDriver::LIGHT);
  }
  return animating ? ListMenu::TICK_MS : 500;
}

bool MessageThreadApplet::onInput(InputEvent ev) {
  int n = _svc ? _svc->messageCount(_key) : 0;

  if (_menuOpen) {
    if (_menu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      int sel = _menu.selected();
      if (_chatMenu) {
        if (sel == 0) {
          if (_svc) _svc->clearConvo(_key);
          _menuOpen = false;
          if (_host) { _host->postToast("Chat cleared"); _host->pop(); }
        } else {
          _menuOpen = false;
          if (_host) _host->postToast("Not available yet");
        }
      } else {
        if (sel == 1) {
          if (_svc) _svc->deleteMessage(_key, _focus);
          _menuOpen = false;
        } else if (sel == 2 && _msgMenu.hasPath) {
          _menuOpen = false;
          messagePathApplet().setTarget(_key, _focus);
          if (_host) _host->push(&messagePathApplet());
        } else {
          _menuOpen = false;
          if (_host) _host->postToast("Not available yet");
        }
      }
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _menuOpen = false; return true; }
    return true;   // swallow all input while the menu is open
  }

  // Nav decisions use the committed target (not the eased value) to avoid chatter.
  int vp = _bodyH > 0 ? _bodyH : 48;
  int step = vp > 16 ? (vp * 2) / 3 : 12;
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
    }
    return true;
  }
  if (ev == InputEvent::Select && n > 0) {
    MessageView m; bool hasPath = false;
    if (_svc && _svc->getMessage(_key, _focus, m))
      hasPath = (m.kind == KIND_INBOUND) || (m.kind == KIND_OUT_CHAN);
    _msgMenu.hasPath = hasPath;
    _chatMenu = false;
    _menu.setModel(&_msgMenu); _menu.setRowHeight(14); _menu.resetSelection();
    _menuOpen = true;
    return true;
  }
  if (ev == InputEvent::SelectLong) {
    _chatMenu = true;
    _menu.setModel(&_chatMenuModel); _menu.setRowHeight(14); _menu.resetSelection();
    _menuOpen = true;
    return true;
  }
  return false;   // Back bubbles -> pop to chat list
}

MessageThreadApplet& messageThreadApplet() {
  static MessageThreadApplet a;
  return a;
}

} // namespace mishmesh
