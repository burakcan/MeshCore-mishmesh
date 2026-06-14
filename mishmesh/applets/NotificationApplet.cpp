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

  const int W = c.width(), H = c.height();
  c.fillRect(0, 0, W, H, DisplayDriver::DARK);   // clear whatever was behind

  // Inverted header band: source icon + label, with an "open" affordance at the
  // right. For channels the label is the group name; the person is the body line.
  const int HDR = 13;
  c.fillRect(1, 1, W - 2, HDR - 1, DisplayDriver::LIGHT);
  int gh = c.fontHeight(iconFont());  if (gh <= 0) gh = 12;
  int fb = c.fontHeight(fontBody());  if (fb <= 0) fb = 8;
  int hIcon = (uint16_t)(_isChannel ? Icon::Users : Icon::Message);
  c.drawGlyph(iconFont(), 3, 1 + (HDR - 1 - gh) / 2, (uint16_t)hIcon, DisplayDriver::DARK);
  const char* hdr = (_isChannel && _name[0]) ? _name : "New message";
  c.drawTextEllipsized(fontBody(), 18, 1 + (HDR - 1 - fb) / 2, W - 18 - 8, hdr, DisplayDriver::DARK);
  c.drawText(fontBody(), W - 4, 1 + (HDR - 1 - fb) / 2, ">", DisplayDriver::DARK, TextAlign::Right);

  int lh = c.lineHeight(fontBody());  if (lh <= 0) lh = 10;

  // Sender line: DM => contact; channel => the person who posted.
  const char* who = _isChannel ? (_sender[0] ? _sender : "Someone")
                               : (_name[0]   ? _name   : "Unknown");
  int y = HDR + 2;
  c.drawTextEllipsized(fontBody(), 3, y, W - 6, who, DisplayDriver::LIGHT);
  y += lh;

  // Footer: a dotted divider above either the unread tally or the dismiss hint.
  const int FOOT = 12;
  int footY = H - FOOT;
  int cap = c.fontHeight(fontCaption()); if (cap <= 0) cap = 6;

  // Preview body, wrapped and clipped to the band between the sender and footer.
  {
    int pvH = (footY - 2) - y;
    if (pvH > 0) {
      Canvas pv = c.region(3, y, W - 6, pvH);
      pv.drawTextWrapped(fontBody(), 0, 0, W - 6, _preview, DisplayDriver::LIGHT);
    }
  }

  c.fillStipple(3, footY - 2, W - 6, 1, DisplayDriver::LIGHT);
  if (_otherUnread > 0) {
    char b[24];
    snprintf(b, sizeof(b), "%u more unread", (unsigned)_otherUnread);
    c.drawGlyph(iconFont(), 3, footY + (FOOT - gh) / 2, (uint16_t)Icon::Mail, DisplayDriver::LIGHT);
    c.drawText(fontCaption(), 16, footY + (FOOT - cap) / 2, b, DisplayDriver::LIGHT);
  } else {
    c.drawText(fontCaption(), 3, footY + (FOOT - cap) / 2, "Back to dismiss", DisplayDriver::LIGHT);
  }

  c.drawRoundRect(0, 0, W, H, DisplayDriver::LIGHT);   // rounded outer frame on top

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
