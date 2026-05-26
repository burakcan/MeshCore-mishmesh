#include <mishmesh/applets/TelemetryApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/TelemetryDecode.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const uint32_t TELEM_TIMEOUT_MS = 12000;

TelemetryApplet::TelemetryApplet()
    : Applet("Telemetry"), _host(nullptr), _svc(nullptr),
      _startSeq(0), _startMs(0), _arrived(false), _scrollTop(0) {
  _pubkey[0] = 0; _view.valid = false; _view.count = 0;
}

void TelemetryApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _arrived = false;
  _scrollTop = 0;
  _view.valid = false; _view.count = 0;
  _startSeq = _svc ? _svc->telemetrySeq() : 0;
  _startMs = 0;   // set on first render via Canvas::now()
  if (_svc) _svc->requestTelemetry(_pubkey);
}

const char* TelemetryApplet::label(int i) const {
  if (i < 0 || i >= _view.count) return "";
  snprintf(_rowLabel, sizeof(_rowLabel), "%s ch%d",
           telemTypeName(_view.fields[i].type), _view.fields[i].channel);
  return _rowLabel;
}
const char* TelemetryApplet::value(int i) const {
  if (i < 0 || i >= _view.count) return "";
  formatField(_view.fields[i], _rowValue, sizeof(_rowValue));
  return _rowValue;
}

int TelemetryApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  if (_startMs == 0) _startMs = c.now();
  c.drawText(fontBody(), 2, 0, "Telemetry", DisplayDriver::LIGHT);
  c.fillRect(0, c.lineHeight(fontBody()), w, 1, DisplayDriver::LIGHT);
  int bodyY = c.lineHeight(fontBody()) + 2;

  if (!_arrived && _svc && _svc->telemetrySeq() != _startSeq) {
    if (_svc->latestTelemetry(_pubkey, _view)) _arrived = true;
  }

  if (_arrived) {
    // Read-only field list, drawn plainly (no selection bar). NavUp/Down scroll.
    int rowH = 12;
    int rows = (h - bodyY) / rowH;
    if (_view.count == 0) {
      c.drawText(fontBody(), 2, bodyY, "No fields", DisplayDriver::LIGHT);
      return 500;
    }
    Canvas body = c.region(0, bodyY, w, h - bodyY);
    for (int r = 0; r < rows; r++) {
      int i = _scrollTop + r;
      if (i >= _view.count) break;
      int ry = r * rowH;
      body.drawTextEllipsized(fontBody(), 2, ry, w / 2, label(i), DisplayDriver::LIGHT);
      body.drawTextEllipsized(fontBody(), w - 2, ry, w / 2, value(i), DisplayDriver::LIGHT, TextAlign::Right);
    }
    if (_view.count > rows)   // more-below indicator
      c.drawText(fontBody(), w - 6, h - rowH, _scrollTop + rows < _view.count ? "v" : "^", DisplayDriver::LIGHT);
    return 500;
  }

  bool timedOut = (c.now() - _startMs) > TELEM_TIMEOUT_MS;
  c.drawText(fontBody(), 2, bodyY, timedOut ? "No response" : "Requested...", DisplayDriver::LIGHT);
  return timedOut ? 1000 : 250;
}

bool TelemetryApplet::onInput(InputEvent ev) {
  if (_arrived && ev == InputEvent::NavDown) {
    if (_scrollTop < _view.count - 1) { _scrollTop++; return true; }
  } else if (_arrived && ev == InputEvent::NavUp) {
    if (_scrollTop > 0) { _scrollTop--; return true; }
  }
  return false;   // Back bubbles -> pop
}

TelemetryApplet& telemetryApplet() {
  static TelemetryApplet s_telem;
  return s_telem;
}
void telemetrySetTarget(const uint8_t* pubKey) { telemetryApplet().setTarget(pubKey); }

}  // namespace mishmesh
