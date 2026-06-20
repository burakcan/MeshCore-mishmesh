// mishmesh/applets/NotificationApplet.cpp
#include "NotificationApplet.h"
#include "MessageThreadApplet.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <cstdio>

namespace mishmesh {

static const uint32_t AUTO_MS = 8000;   // auto-dismiss when left untouched

void NotificationApplet::raise(MessagesService* svc, const ConvoKey& key) {
  _svc = svc;
  _key = key;
  _name[0] = _sender[0] = _preview[0] = 0;
  _isChannel = (key.type == 1);
  _otherUnread = 0;
  _raisedAt = 0;   // restart the auto-dismiss timer on the next render
  if (!svc) return;

  // One pass over the chat list: pull the subject's display name and, in the same
  // loop, tally how many *other* chats are still unread (the footer's count).
  ConvoView v;
  int n = svc->convoCount();
  for (int i = 0; i < n; i++) {
    if (!svc->getConvo(i, v)) continue;
    if (v.key.equals(key)) {
      snprintf(_name, sizeof(_name), "%s", v.name ? v.name : "");
      _isChannel = v.isChannel;
    } else if (v.unread > 0) {
      _otherUnread++;
    }
  }

  // Latest message: body for the preview, and (channels only) the sender name.
  int mc = svc->messageCount(key);
  if (mc > 0) {
    MessageView m;
    if (svc->getMessage(key, mc - 1, m)) {
      snprintf(_preview, sizeof(_preview), "%s", m.text ? m.text : "");
      if (m.senderName) snprintf(_sender, sizeof(_sender), "%s", m.senderName);
    }
  }
}

void NotificationApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  if (!_svc) _svc = ctx.messages;   // raise() normally sets it first
  _raisedAt = 0;
}

void NotificationApplet::onForeground() { _raisedAt = 0; }   // restart timer when re-shown

int NotificationApplet::onRender(Canvas& c) {
  uint32_t now = c.now();
  if (_raisedAt == 0) _raisedAt = now ? now : 1;
  if (now - _raisedAt >= AUTO_MS) { if (_host) _host->pop(); return 0; }

  c.fillRect(0, 0, c.width(), c.height(), DisplayDriver::DARK);   // clear whatever was behind

  // Inset the card so it reads as a floating modal rather than a full screen.
  // Vertical inset is kept small so two wrapped preview lines still fit.
  const int MX = 6, MY = 2;
  Canvas card = c.region(MX, MY, c.width() - 2 * MX, c.height() - 2 * MY);
  const int W = card.width(), H = card.height();

  // Inverted header band: source icon + label, with an "open" affordance at the
  // right. For channels the label is the group name; the person is the body line.
  const int HDR = 13;
  card.fillRect(1, 1, W - 2, HDR - 1, DisplayDriver::LIGHT);
  int gh = card.fontHeight(iconFont());  if (gh <= 0) gh = 12;
  int fb = card.fontHeight(fontBody());  if (fb <= 0) fb = 8;
  int hIcon = (uint16_t)(_isChannel ? Icon::Users : Icon::Message);
  card.drawGlyph(iconFont(), 3, 1 + (HDR - 1 - gh) / 2, (uint16_t)hIcon, DisplayDriver::DARK);
  const char* hdr = (_isChannel && _name[0]) ? _name : "New message";
  card.drawTextEllipsized(fontBody(), 18, 1 + (HDR - 1 - fb) / 2, W - 18 - 8, hdr, DisplayDriver::DARK);
  card.drawText(fontBody(), W - 4, 1 + (HDR - 1 - fb) / 2, ">", DisplayDriver::DARK, TextAlign::Right);

  int lh = card.lineHeight(fontBody());  if (lh <= 0) lh = 10;

  // Sender line: DM => contact; channel => the person who posted.
  const char* who = _isChannel ? (_sender[0] ? _sender : "Someone")
                               : (_name[0]   ? _name   : "Unknown");
  int y = HDR + 2;
  card.drawTextEllipsized(fontBody(), 3, y, W - 6, who, DisplayDriver::LIGHT);
  y += lh;

  // Footer: a dotted divider above either the unread tally or the dismiss hint.
  const int FOOT = 12;
  int footY = H - FOOT;
  int cap = card.fontHeight(fontCaption()); if (cap <= 0) cap = 6;

  // Preview body, wrapped and clipped to the band between the sender and footer.
  {
    int pvH = (footY - 2) - y;
    if (pvH > 0) {
      Canvas pv = card.region(3, y, W - 6, pvH);
      pv.drawTextWrapped(fontBody(), 0, 0, W - 6, _preview, DisplayDriver::LIGHT);
    }
  }

  card.fillStipple(3, footY - 2, W - 6, 1, DisplayDriver::LIGHT);
  if (_otherUnread > 0) {
    char b[24];
    snprintf(b, sizeof(b), "%u more unread", (unsigned)_otherUnread);
    card.drawGlyph(iconFont(), 3, footY + (FOOT - gh) / 2, (uint16_t)Icon::Mail, DisplayDriver::LIGHT);
    card.drawText(fontCaption(), 16, footY + (FOOT - cap) / 2, b, DisplayDriver::LIGHT);
  } else {
    card.drawText(fontCaption(), 3, footY + (FOOT - cap) / 2, "Back to dismiss", DisplayDriver::LIGHT);
  }

  card.drawRoundRect(0, 0, W, H, DisplayDriver::LIGHT);   // rounded outer frame on top

  uint32_t shown = now - _raisedAt;
  return (int)(shown < AUTO_MS ? AUTO_MS - shown : 0);   // wake to auto-dismiss
}

bool NotificationApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select || ev == InputEvent::SelectLong) {
    messageThreadApplet().setTarget(_key);
    if (_host) _host->replace(&messageThreadApplet());   // banner -> chat, in place
    return true;
  }
  if (ev == InputEvent::Back || ev == InputEvent::Cancel) return false;  // bubble -> host pops (dismiss)
  return true;   // modal: swallow nav etc. so stray presses don't leak through
}

NotificationApplet& notificationApplet() {
  static NotificationApplet a;
  return a;
}

}  // namespace mishmesh
