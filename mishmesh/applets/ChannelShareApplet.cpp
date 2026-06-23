// mishmesh/applets/ChannelShareApplet.cpp
#include "ChannelShareApplet.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

void ChannelShareApplet::setTarget(const ConvoKey& key, const char* name) {
  _key = key;
  strncpy(_name, name ? name : "", sizeof(_name) - 1);
  _name[sizeof(_name) - 1] = 0;
}

void ChannelShareApplet::onStart(AppletContext& ctx) {
  _svc = ctx.messages;
  _showKey = false;
  _keyHex[0] = 0;
  if (_svc) _svc->channelKeyHex(_key, _keyHex, sizeof(_keyHex));
  buildUri();
  _qr.build(_uri);
}

// meshcore://channel/add?name=<url-encoded name>&secret=<hex>. The name is
// percent-encoded (RFC 3986 unreserved set passes through) so spaces and the
// '#' of hashtag channels survive a URI parse.
void ChannelShareApplet::buildUri() {
  static const char PREFIX[] = "meshcore://channel/add?name=";
  static const char MID[]    = "&secret=";
  static const char HEX[]    = "0123456789ABCDEF";
  const int cap = (int)sizeof(_uri) - 1;
  int p = 0;

  for (const char* s = PREFIX; *s && p < cap; ++s) _uri[p++] = *s;
  for (const char* s = _name; *s && p < cap - 3; ++s) {
    unsigned char ch = (unsigned char)*s;
    bool unreserved = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                      (ch >= '0' && ch <= '9') ||
                      ch == '-' || ch == '_' || ch == '.' || ch == '~';
    if (unreserved) {
      _uri[p++] = (char)ch;
    } else {
      _uri[p++] = '%';
      _uri[p++] = HEX[ch >> 4];
      _uri[p++] = HEX[ch & 0x0F];
    }
  }
  for (const char* s = MID; *s && p < cap; ++s) _uri[p++] = *s;
  for (const char* s = _keyHex; *s && p < cap; ++s) _uri[p++] = *s;
  _uri[p] = 0;
}

int ChannelShareApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  // No title bar: the QR (or key) fills a full-height square anchored top-left,
  // and the channel name + toggle hint sit in the leftover column on the right.
  const int side = (h <= w) ? h : w;
  const int rightX = side + 3;
  const bool hasRight = rightX < w;

  if (!_keyHex[0]) {
    c.drawText(fontBody(), side / 2, h / 2 - 4, "No key",
               DisplayDriver::LIGHT, TextAlign::Center);
  } else if (_showKey) {
    c.drawTextWrapped(fontBody(), 1, 2, side - 2, _keyHex, DisplayDriver::LIGHT);
  } else if (_qr.valid()) {
    _qr.draw(c, 0, 0, side, side);
  } else {
    c.drawText(fontBody(), side / 2, h / 2 - 4, "QR too big",
               DisplayDriver::LIGHT, TextAlign::Center);
  }

  if (hasRight) {
    Canvas right = c.region(rightX, 0, w - rightX, h);
    int ry = 2;
    if (_name[0])
      ry = right.drawTextWrapped(fontBody(), 1, ry, right.width() - 2, _name,
                                 DisplayDriver::LIGHT) + 4;
    const char* hint = _showKey ? "Center to show QR" : "Center to show key";
    right.drawTextWrapped(fontCaption(), 1, ry, right.width() - 2, hint,
                          DisplayDriver::LIGHT);
  }
  return 500;
}

bool ChannelShareApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select) { _showKey = !_showKey; return true; }
  return false;   // Back bubbles -> host pops
}

ChannelShareApplet& channelShareApplet() {
  static ChannelShareApplet a;
  return a;
}

}  // namespace mishmesh
