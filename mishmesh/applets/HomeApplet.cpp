#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Metrics.h>
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

// The hint bar is the recessive tier on a 64px panel, but caption type is too
// faint to read on a larger one - promote it where there is room to spare.
static const Font* hintFont(const Canvas& c) { return tierFont(c); }

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
  // Clock-engine indicators: armed alarm, pending timer, pomodoro session,
  // running stopwatch.
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
  if (clockService().pmActive()) {
    xr -= 12;
    c.drawGlyph(iconFont(), xr, 0, (uint16_t)Icon::Tomato, DisplayDriver::LIGHT);
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
  // fontNum is 16px, sized for a 64px-tall panel. On a taller one it reads as an
  // afterthought, so magnify it - whole multiples of a bitmap glyph are exact, so
  // the clock stays as sharp as everything around it. The vertical budget still
  // has to clear the hint bar: clock + date + unread all sit above it. Width is
  // its own limit: a 122px portrait panel is tall enough to ask for scale 2 and
  // too narrow to hold it, which used to draw "16:00" as a clipped "16:0".
  const int numAvail = w - 2 * MARGIN - (suffix ? c.textWidth(fontBody(), suffix) + 3 : 0);
  const int numScale = c.fitScale(fontNum(), clock, numAvail, numScaleCap(c));
  const int numH = c.fontHeightScaled(fontNum(), numScale);
  int cy = BAR_H + 4;
  c.drawTextScaled(fontNum(), MARGIN, cy, clock, DisplayDriver::LIGHT, numScale);
  if (suffix) {
    int sx = MARGIN + c.textWidthScaled(fontNum(), clock, numScale) + 3;
    int sy = cy + numH - c.fontHeight(fontBody());   // baseline-align
    c.drawText(fontBody(), sx, sy, suffix, DisplayDriver::LIGHT);
  }
  int dy = cy + numH + 2;
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
    int hintY = h - c.fontHeight(hintFont(c)) - 1;
    if (uy + 13 <= hintY) {   // 12px glyph + 1px breathing room above the hint bar
      c.drawGlyph(iconFont(), MARGIN, uy, (uint16_t)Icon::Mail, DisplayDriver::LIGHT);
      c.drawText(fontBody(), MARGIN + 16, uy + 2, b, DisplayDriver::LIGHT);
    }
  }

  // --- hint bar: <Left  .Apps  vTgls  Right> ---
  {
    const Font* f = hintFont(c);
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
    const int fh = c.fontHeight(f);
    int y = h - fh - 1;
    // The markers are drawn at a fixed 5px (3px for the press square) while the
    // label height follows the font, so centre them or they ride high.
    const int gy = y + (fh - 5) / 2;
    const int sy = y + (fh - 3) / 2;
    if (lr) {
      triLeft(c, x, gy);
      c.drawText(f, x + 3 + PAD, y, l4, DisplayDriver::LIGHT);
      x += wl + GAP;
    }
    if (_menu) {
      c.fillRect(x, sy, 3, 3, DisplayDriver::LIGHT);   // press marker
      c.drawText(f, x + 3 + PAD, y, "Apps", DisplayDriver::LIGHT);
      x += wa + GAP;
    }
    triDown(c, x, gy);
    c.drawText(f, x + GLYPH + PAD, y, "Tgls", DisplayDriver::LIGHT);
    x += wt;
    if (rr) {
      x += GAP;
      c.drawText(f, x, y, r4, DisplayDriver::LIGHT);
      triRight(c, x + c.textWidth(f, r4) + PAD, gy);
    }
  }

  int delay = 1000;
  if (_drawer.isOpen()) {
    _drawer.draw(c);
    delay = _drawer.animating() ? 33 : 500;
  }

  if (_sleepArmed) {
    const uint32_t elapsed = c.now() - _lastBackMs;
    const uint32_t window = lockTapWindow();
    if (elapsed >= window) {
      _sleepArmed = false;
      _backTaps = 0;
      if (_host) _host->requestSleep();
    } else {
      const int remaining = (int)(window - elapsed);
      if (remaining < delay) delay = remaining;
    }
  }
  return delay;
}

bool HomeApplet::onInput(InputEvent ev) {
  if (_drawer.isOpen()) return _drawer.onInput(ev);

  // Back sleeps the device; three in quick succession lock it instead. Both have
  // to wait out the tap window before acting - see _sleepArmed.
  if (ev == InputEvent::Back) {
    uint32_t now = _host ? _host->nowMs() : 0;
    if (_backTaps > 0 && now - _lastBackMs > lockTapWindow()) _backTaps = 0;
    _backTaps++;
    _lastBackMs = now;
    _sleepArmed = true;
    if (_backTaps >= 3 && _lock && _host) {
      _backTaps = 0;
      _sleepArmed = false;
      _host->push(_lock);
    }
    return true;
  }
  _backTaps = 0;   // any other input breaks the triple-Back sequence
  _sleepArmed = false;

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
