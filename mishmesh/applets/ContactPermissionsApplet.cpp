#include <mishmesh/applets/ContactPermissionsApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

static const char* ROW_LABELS[ContactPermissionsApplet::ROW_COUNT] = {
  "Allow telemetry", "Include location", "Include sensors",
};

ContactPermissionsApplet::ContactPermissionsApplet()
    : Applet("Permissions"), _host(nullptr), _app(nullptr), _svc(nullptr), _perms(0) {
  _pubkey[0] = 0; _name[0] = 0;
}

uint8_t ContactPermissionsApplet::bitFor(int row) {
  switch (row) {
    case AllowRequests:   return TelemPermBase;
    case IncludeLocation: return TelemPermLocation;
    case IncludeSensors:  return TelemPermEnvironment;
    default:              return 0;
  }
}

void ContactPermissionsApplet::setTarget(const uint8_t* pubKey, const char* name) {
  memcpy(_pubkey, pubKey, 6);
  strncpy(_name, name ? name : "", sizeof(_name) - 1);
  _name[sizeof(_name) - 1] = 0;
}

const char* ContactPermissionsApplet::label(int i) const {
  return (i >= 0 && i < ROW_COUNT) ? ROW_LABELS[i] : "";
}

bool ContactPermissionsApplet::toggleState(int i) const {
  return (_perms & bitFor(i)) != 0;
}

void ContactPermissionsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app = ctx.app;
  _svc = ctx.contacts;
  _perms = _svc ? _svc->getTelemetryPerms(_pubkey) : 0;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();   // shared static applet: always reopen at the first row
}

int ContactPermissionsApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle(_name);
  if (_app) _bar.setBattery(_app->batteryMillivolts());
  _bar.draw(c, 0, 0, w, barH);
  _list.draw(c, 0, barH, w, h - barH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool ContactPermissionsApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _svc) {
    uint8_t bit = bitFor(_list.selected());
    bool on = !(_perms & bit);
    if (_svc->setTelemetryPerm(_pubkey, bit, on)) {
      if (on) _perms |= bit; else _perms &= ~bit;   // mirror the pill on next render
    }
    return true;
  }
  return false;   // Back bubbles -> host pops back to the contact detail
}

ContactPermissionsApplet& contactPermissionsApplet() {
  static ContactPermissionsApplet s_perms;
  return s_perms;
}

}  // namespace mishmesh
