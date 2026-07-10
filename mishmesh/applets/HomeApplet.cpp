#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ClockService.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const int BAR_H = 12;
static const int MARGIN = 4;

void HomeApplet::hintLabel(const char* label, char out[5]) {
  int n = 0;
  if (label) for (; label[n] && n < 4; n++) out[n] = label[n];
  out[n] = 0;
}

void HomeApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app = ctx.app;
  _msgs = ctx.messages;
  _sound = ctx.sound;
  _drawer.bind(ctx);
}

// Small solid arrows for the hint bar (caption-font scale).
static void triLeft(Canvas& c, int x, int y) {
  c.fillRect(x, y + 2, 1, 1, DisplayDriver::LIGHT);
  c.fillRect(x + 1, y + 1, 1, 3, DisplayDriver::LIGHT);
  c.fillRect(x + 2, y, 1, 5, DisplayDriver::LIGHT);
}
static void triRight(Canvas& c, int x, int y) {
  c.fillRect(x, y, 1, 5, DisplayDriver::LIGHT);
  c.fillRect(x + 1, y + 1, 1, 3, DisplayDriver::LIGHT);
  c.fillRect(x + 2, y + 2, 1, 1, DisplayDriver::LIGHT);
}
static void triDown(Canvas& c, int x, int y) {
  c.fillRect(x, y + 1, 5, 1, DisplayDriver::LIGHT);
  c.fillRect(x + 1, y + 2, 3, 1, DisplayDriver::LIGHT);
  c.fillRect(x + 2, y + 3, 1, 1, DisplayDriver::LIGHT);
}

int HomeApplet::onRender(Canvas& c) {
  int w = c.width();
  int h = c.height();

  // --- top bar: name left, exceptional-state icons + battery right ---
  c.fillRect(0, BAR_H - 1, w, 1, DisplayDriver::LIGHT);
  _batt.setMillivolts(_app ? _app->batteryMillivolts() : 0);
  int xr = w - 2;
  // The bar occupies y [0, BAR_H) of the root canvas, so draw on c directly -
  // region() returns a temporary, which can't bind to the Canvas& parameter.
  xr -= _batt.drawRightAligned(c, xr, BAR_H - 1) + 3;
  if (_sound && _sound->volume() == sound::VolumeLevel::Mute) {
    xr -= 12;
    c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::VolumeMute, DisplayDriver::LIGHT);
    xr -= 3;
  }
  if (_app && _app->gpsEnabled()) {
    xr -= 12;
    if (_app->gpsHasFix()) {   // inverted chip = has a fix (searching = outline)
      c.fillRect(xr - 1, 0, 14, BAR_H - 2, DisplayDriver::LIGHT);
      c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Gps, DisplayDriver::DARK);
    } else {
      c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Gps, DisplayDriver::LIGHT);
    }
    xr -= 3;
  }
  if (_app && _app->bleEnabled()) {
    xr -= 12;
    if (_app->bleConnected()) {   // inverted chip = client attached
      c.fillRect(xr - 1, 0, 14, BAR_H - 2, DisplayDriver::LIGHT);
      c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Bluetooth, DisplayDriver::DARK);
    } else {
      c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Bluetooth, DisplayDriver::LIGHT);
    }
    xr -= 3;
  }
  if (_app && _app->repeaterMode()) {
    xr -= 12;
    c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Radio, DisplayDriver::LIGHT);
    xr -= 3;
  }
  // Clock-engine indicators: armed alarm, pending timer, running stopwatch.
  if (clockService().alarmEnabled()) {
    xr -= 12;
    c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::AlarmClock, DisplayDriver::LIGHT);
    xr -= 3;
  }
  if (clockService().tmRunning() || clockService().tmPaused()) {
    xr -= 12;
    c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Hourglass, DisplayDriver::LIGHT);
    xr -= 3;
  }
  if (clockService().swRunning()) {
    xr -= 12;
    c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Clock, DisplayDriver::LIGHT);
    xr -= 3;
  }
  int ty = (BAR_H - 1 - c.fontHeight(fontBody())) / 2;
  if (ty < 0) ty = 0;
  if (xr - 4 > 0)
    c.drawTextEllipsized(fontBody(), 2, ty, xr - 4,
                         _app ? _app->nodeName() : "", DisplayDriver::LIGHT);

  // --- clock / date / unread, left-aligned ---
  uint32_t t = _app ? _app->epochSeconds() : 0;
  int16_t off = _app ? _app->tzOffsetMinutes() : 0;
  LocalTime lt = applyTz(t, off);
  char clock[12];
  formatClock(clock, sizeof(clock), lt, _app ? _app->timeFormat12h() : false);
  // fontNum's atlas is digits/':'/'.' only - split the 12h AM/PM suffix off and
  // render it in fontBody, or it comes out as missing-glyph blocks.
  char* suffix = strchr(clock, ' ');
  if (suffix) *suffix++ = 0;
  // Vertical budget assumes the compact 16px fontNum: clock+date+unread must clear the hint bar.
  int cy = BAR_H + 4;
  c.drawText(fontNum(), MARGIN, cy, clock, DisplayDriver::LIGHT);
  if (suffix) {
    int sx = MARGIN + c.textWidth(fontNum(), clock) + 3;
    int sy = cy + c.fontHeight(fontNum()) - c.fontHeight(fontBody());   // baseline-align
    c.drawText(fontBody(), sx, sy, suffix, DisplayDriver::LIGHT);
  }
  int dy = cy + c.fontHeight(fontNum()) + 2;
  if (t) {
    char date[24];
    DateFormat df = _app ? (DateFormat)_app->dateFormat() : DateFormat::DMY;
    formatShortDate(date, sizeof(date), lt, df);
    c.drawText(fontBody(), MARGIN, dy, date, DisplayDriver::LIGHT);
  }
  if (_msgs && _msgs->totalNotifyUnread()) {
    char b[8];
    snprintf(b, sizeof(b), "%u", _msgs->totalNotifyUnread());
    int uy = dy + c.fontHeight(fontBody()) + 2;
    int hintY = h - c.fontHeight(fontCaption()) - 1;
    if (uy + 13 <= hintY) {   // 12px glyph + 1px breathing room above the hint bar
      c.drawGlyph(iconFont(), MARGIN, uy, (uint16_t)Icon::Mail, DisplayDriver::LIGHT);
      c.drawText(fontBody(), MARGIN + 16, uy + 2, b, DisplayDriver::LIGHT);
    }
  }

  // --- hint bar: <Left  .Apps  vTgls  Right> in the caption tier ---
  {
    const Font* f = fontCaption();
    char l4[5] = {0}, r4[5] = {0};
    const AppletRegistration* lr = uiPrefs().quickAction(UiPrefs::SLOT_LEFT);
    const AppletRegistration* rr = uiPrefs().quickAction(UiPrefs::SLOT_RIGHT);
    if (lr) hintLabel(lr->label, l4);
    if (rr) hintLabel(rr->label, r4);
    // measure the run: [<l4] [#Apps] [vTgls] [r4>]
    const int GLYPH = 5, PAD = 2;
    int GAP = 8;
    int wl = lr ? 3 + PAD + c.textWidth(f, l4) : 0;
    int wa = _menu ? 3 + PAD + c.textWidth(f, "Apps") : 0;
    int wt = GLYPH + PAD + c.textWidth(f, "Tgls");
    int wr = rr ? c.textWidth(f, r4) + PAD + 3 : 0;
    int total = wl + wa + wt + wr;
    int gaps = (lr ? 1 : 0) + (_menu ? 1 : 0) + (rr ? 1 : 0);
    if (gaps > 0 && total + gaps * GAP > w) {
      GAP = (w - total) / gaps;
      if (GAP < 2) GAP = 2;
    }
    total += gaps * GAP;
    int x = (w - total) / 2;
    if (x < 0) x = 0;
    int y = h - c.fontHeight(f) - 1;
    if (lr) {
      triLeft(c, x, y);
      c.drawText(f, x + 3 + PAD, y, l4, DisplayDriver::LIGHT);
      x += wl + GAP;
    }
    if (_menu) {
      c.fillRect(x, y + 1, 3, 3, DisplayDriver::LIGHT);   // press marker
      c.drawText(f, x + 3 + PAD, y, "Apps", DisplayDriver::LIGHT);
      x += wa + GAP;
    }
    triDown(c, x, y);
    c.drawText(f, x + GLYPH + PAD, y, "Tgls", DisplayDriver::LIGHT);
    x += wt;
    if (rr) {
      x += GAP;
      c.drawText(f, x, y, r4, DisplayDriver::LIGHT);
      triRight(c, x + c.textWidth(f, r4) + PAD, y);
    }
  }

  if (_drawer.isOpen()) {
    _drawer.draw(c);
    return _drawer.animating() ? 33 : 500;
  }
  return 1000;
}

bool HomeApplet::onInput(InputEvent ev) {
  if (_drawer.isOpen()) return _drawer.onInput(ev);

  // Triple-Back locks the screen. Back is otherwise a no-op at the root, so
  // consuming it here changes nothing for single/double presses.
  if (ev == InputEvent::Back) {
    uint32_t now = _host ? _host->nowMs() : 0;
    if (_backTaps > 0 && now - _lastBackMs > LOCK_TAP_WINDOW_MS) _backTaps = 0;
    _backTaps++;
    _lastBackMs = now;
    if (_backTaps >= 3 && _lock && _host) {
      _backTaps = 0;
      _host->push(_lock);
    }
    return true;
  }
  _backTaps = 0;   // any other input breaks the triple-Back sequence

  switch (ev) {
    case InputEvent::Select:
      if (_menu && _host) { _host->push(_menu); return true; }
      return false;
    case InputEvent::NavDown:
      _drawer.open();
      return true;
    case InputEvent::NavLeft:
    case InputEvent::NavRight: {
      const AppletRegistration* r = uiPrefs().quickAction(
          ev == InputEvent::NavLeft ? UiPrefs::SLOT_LEFT : UiPrefs::SLOT_RIGHT);
      if (r && _host) { _host->push(r->applet); return true; }
      return false;
    }
    default:
      return false;   // NavUp reserved; Back bubbles (root)
  }
}

}  // namespace mishmesh
