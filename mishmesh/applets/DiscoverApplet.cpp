#include <mishmesh/applets/DiscoverApplet.h>
#include <mishmesh/applets/DiscoverDetailApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactFormat.h>              // kindIcon
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

const char* DiscoverApplet::Model::label(int i) const {
  if (i == 0) return "Discover Repeaters";
  if (i == 1) return "Discover Sensors";
  static char buf[16];
  ContactsService::DiscoverResultView r;
  if (!_svc || !_svc->getDiscoverResult(i - 2, r)) return "";
  snprintf(buf, sizeof(buf), "%02X%02X%02X", r.pubKey[0], r.pubKey[1], r.pubKey[2]);
  return buf;
}

uint16_t DiscoverApplet::Model::icon(int i) const {
  if (i == 0) return (uint16_t)Icon::Radio;
  if (i == 1) return (uint16_t)Icon::Chip;
  ContactsService::DiscoverResultView r;
  if (!_svc || !_svc->getDiscoverResult(i - 2, r)) return 0;
  return kindIcon((ContactKind)r.type);
}

const char* DiscoverApplet::Model::value(int i) const {
  if (i < 2) return nullptr;
  static char buf[12];
  ContactsService::DiscoverResultView r;
  if (!_svc || !_svc->getDiscoverResult(i - 2, r)) return nullptr;
  int snrDb = (r.snrX4 + (r.snrX4 >= 0 ? 2 : -2)) / 4;   // round to nearest dB, not truncate
  snprintf(buf, sizeof(buf), "%d dB", snrDb);
  return buf;
}

DiscoverApplet::DiscoverApplet() : Applet("Discover") {}

void DiscoverApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _svc = ctx.contacts; _host = ctx.host;
  _model.bind(_svc);
  _list.setModel(&_model);
  _list.setRowHeight(14);
  _list.resetSelection();
  _seenSeq = _svc ? _svc->discoverSeq() : 0;
}

int DiscoverApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  // A response can arrive between input events; refresh selection bookkeeping when
  // the service reports new results.
  if (_svc && _svc->discoverSeq() != _seenSeq) _seenSeq = _svc->discoverSeq();

  const char* status = scanning() ? "Scanning..."
                     : (_svc && _svc->discoverResultCount() > 0 ? nullptr
                        : "No nodes found - press to scan");
  int top = 0;
  if (status) {
    c.drawText(fontBody(), 2, 2, status, DisplayDriver::LIGHT);
    top = 14;
  }
  _list.draw(c, 0, top, w, h - top);
  return (scanning() || _list.needsAnimation()) ? ListMenu::TICK_MS : 500;
}

void DiscoverApplet::startScan(uint8_t advTypeMask) {
  if (scanning() || !_svc) return;                 // one request per window
  if (_svc->startNodeDiscover(advTypeMask)) {
    _seenSeq = _svc->discoverSeq();
    if (_host) _host->postToast("Discovery sent");
  }
}

bool DiscoverApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int sel = _list.selected();
    if (sel == 0) { startScan(0x04); return true; }   // repeaters
    if (sel == 1) { startScan(0x10); return true; }   // sensors
    if (_svc) {
      ContactsService::DiscoverResultView r;
      if (_svc->getDiscoverResult(sel - 2, r)) {
        // A responder we already hold belongs on its contact page; the discover detail
        // screen only offers "Add to contacts", which would read as un-added.
        if (_svc->isContact(r.pubKey)) {
          contactDetailApplet().setTarget(r.pubKey);
          if (_host) _host->push(&contactDetailApplet());
          return true;
        }
        ContactView v{};
        v.name = "";
        v.type = r.type;
        v.pubKey = r.pubKey;      // copied immediately by setTarget
        v.hasPath = false; v.hops = 0; v.lastAdvert = 0;
        v.hasLocation = false;
        discoverDetailApplet().setTarget(v);
        if (_host) _host->push(&discoverDetailApplet());
      }
    }
    return true;
  }
  return false;   // Back bubbles to the host
}

DiscoverApplet& discoverApplet() {
  static DiscoverApplet s_discover;
  return s_discover;
}

}  // namespace mishmesh
