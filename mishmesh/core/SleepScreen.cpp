#include <mishmesh/core/SleepScreen.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/BatteryIndicator.h>
#include <mishmesh/applets/onboarding_logo.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const int MARGIN = 4;

static int drawOff(Canvas&, const AppletContext&) {
  return SLEEP_NEVER;   // the host already cleared to the background
}

static int drawLogo(Canvas& c, const AppletContext&) {
  const int gap = 5;    // same stacking as UITask::drawBootSplash
  const int blockH = MESHCORE_LOGO_H + gap + MISHMESH_LOGO_H;
  const int top = (c.height() - blockH) / 2;
  c.drawXbm((c.width() - MESHCORE_LOGO_W) / 2, top,
            MESHCORE_LOGO, MESHCORE_LOGO_W, MESHCORE_LOGO_H, DisplayDriver::LIGHT);
  c.drawXbm((c.width() - MISHMESH_LOGO_W) / 2, top + MESHCORE_LOGO_H + gap,
            MISHMESH_LOGO, MISHMESH_LOGO_W, MISHMESH_LOGO_H, DisplayDriver::LIGHT);
  return SLEEP_NEVER;
}

// Node name, unread, link state and battery along the bottom edge.
static void drawStatusStrip(Canvas& c, const AppletContext& ctx, int y, int h) {
  Canvas strip = c.region(0, y, c.width(), h);
  const AppServices* app = ctx.app;

  int xr = strip.width() - MARGIN;
  BatteryIndicator batt;
  batt.setMillivolts(app ? app->batteryMillivolts() : 0);
  xr -= batt.drawRightAligned(strip, xr, h);

  if (ctx.host != nullptr && ctx.host->deviceLocked()) {
    xr -= 15;
    strip.drawGlyph(iconFont(), xr, (h - 12) / 2, (uint16_t)Icon::Lock, DisplayDriver::LIGHT);
  }

  if (app && app->bleConnected()) {
    xr -= 15;
    strip.drawGlyph(iconFont(), xr, (h - 12) / 2, (uint16_t)Icon::Bluetooth, DisplayDriver::LIGHT);
  }

  uint16_t unread = ctx.messages ? ctx.messages->totalNotifyUnread() : 0;
  if (unread) {
    char b[8];
    snprintf(b, sizeof(b), "%u", (unsigned)unread);
    const int bodyH = strip.fontHeight(fontBody());
    xr -= strip.textWidth(fontBody(), b) + 3;
    strip.drawText(fontBody(), xr, (h - bodyH) / 2, b, DisplayDriver::LIGHT);
    xr -= 15;
    strip.drawGlyph(iconFont(), xr, (h - 12) / 2, (uint16_t)Icon::Mail, DisplayDriver::LIGHT);
  }

  if (xr - MARGIN - 4 > 0) {
    const int bodyH = strip.fontHeight(fontBody());
    strip.drawTextEllipsized(fontBody(), MARGIN, (h - bodyH) / 2, xr - MARGIN - 4,
                             app ? app->nodeName() : "", DisplayDriver::LIGHT);
  }
}

static int drawClock(Canvas& c, const AppletContext& ctx) {
  const AppServices* app = ctx.app;
  const uint32_t t = app ? app->epochSeconds() : 0;
  const LocalTime lt = applyTz(t, app ? app->tzOffsetMinutes() : 0);

  char clock[12];
  formatClock(clock, sizeof(clock), lt, app ? app->timeFormat12h() : false);
  // fontNum's atlas is digits/':'/'.' only, so the 12h AM/PM suffix has to be
  // split off and drawn in fontBody or it comes out as missing-glyph blocks.
  char* suffix = strchr(clock, ' ');
  if (suffix) *suffix++ = 0;

  const int bodyH = c.fontHeight(fontBody());
  const int stripH = bodyH + 6;
  const int faceH = c.height() - stripH;

  const int suffixW = suffix ? c.textWidth(fontBody(), suffix) + 3 : 0;
  const int scale = c.fitScale(fontNum(), clock, c.width() - 2 * MARGIN - suffixW, 6);
  const int numH = c.fontHeightScaled(fontNum(), scale);
  const int numW = c.textWidthScaled(fontNum(), clock, scale);

  char date[24];
  date[0] = 0;
  if (t) {
    formatShortDate(date, sizeof(date), lt,
                    app ? (DateFormat)app->dateFormat() : DateFormat::DMY);
  }
  const int dateH = date[0] ? bodyH + 4 : 0;

  int top = (faceH - numH - dateH) / 2;
  if (top < 0) top = 0;
  const int nx = (c.width() - numW - suffixW) / 2;
  c.drawTextScaled(fontNum(), nx, top, clock, DisplayDriver::LIGHT, scale);
  if (suffix) {
    c.drawText(fontBody(), nx + numW + 3, top + numH - bodyH, suffix, DisplayDriver::LIGHT);
  }
  if (date[0]) {
    c.drawTextCentered(fontBody(), 0, top + numH + 4, c.width(), bodyH, date, DisplayDriver::LIGHT);
  }

  c.fillRect(0, c.height() - stripH, c.width(), 1, DisplayDriver::LIGHT);
  drawStatusStrip(c, ctx, c.height() - stripH + 1, stripH - 1);

  // Land on the minute rather than a minute after falling asleep. With no clock
  // set there is no boundary to aim at - tick anyway so the face appears as soon
  // as time syncs; until then it composes an identical frame, which the driver's
  // frame CRC drops before it reaches the panel.
  return t ? (int)(60 - lt.second) * 1000 : 60000;
}

static const SleepScreen SCREENS[SLEEP_SCREEN_COUNT] = {
  { "Screen off", SleepOrient::None,      drawOff },
  // The MeshCore wordmark is 128px wide and a portrait canvas is 122, so this one
  // gets the panel turned for it rather than being cropped or fractionally scaled.
  { "Logo",       SleepOrient::Landscape, drawLogo },
  { "Clock",      SleepOrient::Either,    drawClock },
};

// Auto first, then the Orientation setting's own labels - a sleep face that is
// deliberately upside down relative to the UI is the whole point of a dock.
static const char* const ORIENT_LABELS[SLEEP_ORIENT_COUNT] = {
  "Auto", "Landscape", "Portrait", "Landscape flipped", "Portrait flipped"
};

static const char* const LABELS[SLEEP_SCREEN_COUNT] = {
  SCREENS[0].label, SCREENS[1].label, SCREENS[2].label,
};

const SleepScreen* sleepScreenAt(int idx) {
  return (idx >= 0 && idx < SLEEP_SCREEN_COUNT) ? &SCREENS[idx] : nullptr;
}

const char* sleepScreenLabel(int idx) {
  const SleepScreen* s = sleepScreenAt(idx);
  return s ? s->label : "";
}

const char* const* sleepScreenLabels() { return LABELS; }

const char* const* sleepOrientLabels() { return ORIENT_LABELS; }

int sleepOrientRotation(int faceIdx, int choice, int uiRotation) {
  const SleepScreen* face = sleepScreenAt(faceIdx);
  if (face == nullptr) return -1;
  uiRotation = ((uiRotation % 4) + 4) % 4;
  // A face that reads only one way is turned for automatically, and the user was
  // never asked - so keep the flip the device is held with and change only the
  // parity, rather than deciding on their behalf which way up they meant.
  switch (face->orient) {
    case SleepOrient::Landscape: return (uiRotation & 2) | 0;
    case SleepOrient::Portrait:  return (uiRotation & 2) | 1;
    case SleepOrient::None:      return -1;
    case SleepOrient::Either:    break;
  }
  if (choice <= SLEEP_ORIENT_AUTO || choice >= SLEEP_ORIENT_COUNT) return -1;
  return choice - 1;   // an explicit choice is absolute, flip included
}

}  // namespace mishmesh
