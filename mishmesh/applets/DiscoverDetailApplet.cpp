#include <mishmesh/applets/DiscoverDetailApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/ContactFormat.h>
#include <mishmesh/core/Geo.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const char* ACTION_LABELS[DiscoverDetailApplet::ACTION_COUNT] = {
  "Add to contacts", "View details",
};

DiscoverDetailApplet::DiscoverDetailApplet()
    : Applet("Discovery"), _host(nullptr), _svc(nullptr), _app(nullptr), _type(0),
      _hasPath(false), _hops(0), _lastAdvert(0), _hasLoc(false), _gpsLat(0), _gpsLon(0),
      _distKm(-1), _viewing(false) {
  _pubkey[0] = 0; _name[0] = 0; _displayName[0] = 0; _infoLine[0] = 0;
}

const char* DiscoverDetailApplet::label(int i) const {
  return (i >= 0 && i < ACTION_COUNT) ? ACTION_LABELS[i] : "";
}

void DiscoverDetailApplet::setTarget(const ContactView& v) {
  memcpy(_pubkey, v.pubKey, 6);
  memcpy(_fullKey, v.pubKey, PUBKEY_LEN);
  strncpy(_name, v.name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = 0;
  _type = v.type; _hasPath = v.hasPath; _hops = v.hops; _lastAdvert = v.lastAdvert;
  _hasLoc = v.hasLocation; _gpsLat = v.gpsLat; _gpsLon = v.gpsLon;
}

void DiscoverDetailApplet::buildInfo() {
  _distKm = -1;
  int32_t slat, slon;
  if (_hasLoc && _svc && _svc->selfLocation(slat, slon))
    _distKm = geoDistanceKm(slat, slon, _gpsLat, _gpsLon);

  if (_type == (uint8_t)ContactKind::Repeater)
    snprintf(_displayName, sizeof(_displayName), "%02X %s", _fullKey[0], _name);
  else
    snprintf(_displayName, sizeof(_displayName), "%s", _name);

  char age[12]; age[0] = 0;
  if (_app) {
    uint32_t now = _app->epochSeconds();
    if (now != 0 && _lastAdvert != 0 && now >= _lastAdvert) contactFormatAge(now - _lastAdvert, age, sizeof(age));
  }
  int p = snprintf(_infoLine, sizeof(_infoLine), "%s", contactTypeName(_type));
  if (_distKm >= 0) p += snprintf(_infoLine + p, sizeof(_infoLine) - p, " | %.2fkm", (double)_distKm);
  if (age[0])       p += snprintf(_infoLine + p, sizeof(_infoLine) - p, " | %s", age);
  _card.set(_displayName, _infoLine);

  _details.clear();
  _details.addf("Type: %s", contactTypeName(_type));
  if (_distKm >= 0) _details.addf("Distance: %.2f km", (double)_distKm);
  if (age[0])       _details.addf("Last heard: %s ago", age);
  if (_hasLoc)      _details.addf("GPS: %.5f, %.5f", _gpsLat / 1e6, _gpsLon / 1e6);
  _details.addLine("Public key:");
  for (int r = 0; r < PUBKEY_LEN; r += 8) {
    char hex[20];
    for (int j = 0; j < 8; j++) snprintf(hex + j * 2, 3, "%02X", _fullKey[r + j]);
    _details.addLine(hex);
  }
}

void DiscoverDetailApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _app = ctx.app;
  _viewing = false;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
  buildInfo();
}

int DiscoverDetailApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int cardH = _card.height(c);
  if (_viewing) {
    _details.setHeader(&_card, cardH);
    _details.draw(c, 0, 0, w, h);
  } else {
    _list.setHeader(&_card, cardH);
    _list.draw(c, 0, 0, w, h);
  }
  bool anim = _card.needsAnimation() || (_viewing ? _details.needsAnimation() : _list.needsAnimation());
  return anim ? ListMenu::TICK_MS : 500;
}

bool DiscoverDetailApplet::onInput(InputEvent ev) {
  if (_viewing) {
    if (ev == InputEvent::Back) { _viewing = false; return true; }
    _details.onInput(ev);
    return true;
  }
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _svc) {
    if (_list.selected() == View) { _viewing = true; return true; }
    if (_svc->addDiscovered(_pubkey)) {
      // Hand off to the new contact's detail page in place of this one, so Back still
      // returns to the Discover list rather than to this (now-stale) discovery screen.
      if (_host) _host->postToast("Added to contacts");
      contactDetailApplet().setTarget(_pubkey);
      if (_host) _host->replace(&contactDetailApplet());
    } else if (_host) {
      _host->postToast("Couldn't add contact");
      _host->pop();
    }
    return true;
  }
  return false;   // Back -> pop to the Discover list
}

DiscoverDetailApplet& discoverDetailApplet() {
  static DiscoverDetailApplet s_detail;
  return s_detail;
}

}  // namespace mishmesh
