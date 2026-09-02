#include <mishmesh/applets/ClockApplet.h>
#include <mishmesh/applets/settings/TimeSettingsPanel.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Metrics.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/core/WorldClock.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const int BAR_H = 13;

// "MM:SS.d" under an hour, then "H:MM:SS" (fontNum has no room for both).
static void fmtStopwatch(char* out, size_t cap, uint32_t ms) {
  if (ms >= 3600000u)
    snprintf(out, cap, "%u:%02u:%02u", (unsigned)(ms / 3600000u),
             (unsigned)((ms / 60000u) % 60), (unsigned)((ms / 1000u) % 60));
  else
    snprintf(out, cap, "%02u:%02u.%u", (unsigned)(ms / 60000u),
             (unsigned)((ms / 1000u) % 60), (unsigned)((ms / 100u) % 10));
}

// Countdown rounds up so the display hits 00:00 exactly when the timer fires.
static void fmtCountdown(char* out, size_t cap, uint32_t ms) {
  uint32_t s = (ms + 999u) / 1000u;
  if (s >= 3600u)
    snprintf(out, cap, "%u:%02u:%02u", (unsigned)(s / 3600u),
             (unsigned)((s / 60u) % 60), (unsigned)(s % 60));
  else
    snprintf(out, cap, "%02u:%02u", (unsigned)(s / 60u), (unsigned)(s % 60));
}

// ---- models ----

const char* ClockApplet::AlarmModel::value(int i) const {
  if (i != Time) return nullptr;
  static char buf[12];
  LocalTime lt{};
  lt.hour = clockService().alarmHour();
  lt.minute = clockService().alarmMinute();
  formatClock(buf, sizeof(buf), lt, app && app->timeFormat12h());
  return buf;
}

const char* ClockApplet::WorldModel::label(int i) const {
  if (i >= clockService().cityCount()) return "Add city";
  return worldCity(clockService().cityAt(i)).name;
}

uint16_t ClockApplet::WorldModel::icon(int i) const {
  return i >= clockService().cityCount() ? (uint16_t)Icon::Plus : 0;
}

const char* ClockApplet::WorldModel::value(int i) const {
  if (i >= clockService().cityCount() || !app) return nullptr;
  uint32_t epoch = app->epochSeconds();
  if (epoch == 0) return "--:--";
  static char buf[12];
  int16_t cityOff = worldCityOffsetNow(clockService().cityAt(i), epoch);
  LocalTime city = applyTz(epoch, cityOff);
  formatClock(buf, sizeof(buf), city, app->timeFormat12h());
  // Mark cities already on tomorrow (+) or still on yesterday (-) vs. us.
  LocalTime here = applyTz(epoch, app->tzOffsetMinutes());
  if (city.day != here.day || city.month != here.month) {
    size_t n = strlen(buf);
    if (n + 1 < sizeof(buf)) {
      buf[n] = cityOff > app->tzOffsetMinutes() ? '+' : '-';
      buf[n + 1] = 0;
    }
  }
  return buf;
}

int ClockApplet::PickModel::count() const { return worldCityCount(); }
const char* ClockApplet::PickModel::label(int i) const { return worldCity(i).name; }
const char* ClockApplet::PickModel::value(int i) const {
  static char buf[12];
  formatOffset(buf, sizeof(buf), worldCity(i).baseOffsetMin);
  return buf;
}

const char* ClockApplet::PomoSetupModel::label(int i) const {
  switch (i) {
    case Focus: return "Focus";
    case Short: return "Short break";
    case Long:  return "Long break";
    case SetN:  return "Set";
    default:    return "Auto-advance";
  }
}

const char* ClockApplet::PomoSetupModel::value(int i) const {
  static char buf[8];
  ClockService& s = clockService();
  switch (i) {
    case Focus: snprintf(buf, sizeof(buf), "%um", s.pmFocusMin()); return buf;
    case Short: snprintf(buf, sizeof(buf), "%um", s.pmShortMin()); return buf;
    case Long:  snprintf(buf, sizeof(buf), "%um", s.pmLongMin());  return buf;
    case SetN:  snprintf(buf, sizeof(buf), "%u",  s.pmSetCount()); return buf;
    default:    return nullptr;   // toggle row draws its own state
  }
}

// ---- applet ----

void ClockApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app = ctx.app;
  _alarmModel.app = _app;
  _worldModel.app = _app;
  timeSettings().begin(ctx);
  _tabs.clear();
  _tabs.addTab("Stopwatch", (uint16_t)Icon::Clock);
  _tabs.addTab("Timer", (uint16_t)Icon::Hourglass);
  _tabs.addTab("Pomodoro", (uint16_t)Icon::Tomato);
  _tabs.addTab("Alarm", (uint16_t)Icon::AlarmClock);
  _tabs.addTab("World", (uint16_t)Icon::Globe);
  _tabs.addTab("Settings", (uint16_t)Icon::Settings);
  _tab = 0;
  _editor.open = false;
  _pickingCity = false;
  _alarmList.setModel(&_alarmModel);
  _alarmList.setRowHeight(14);
  _alarmList.resetSelection();   // singleton reuse: setModel skips reset on same-ptr rebind
  _worldList.setModel(&_worldModel);
  _worldList.setRowHeight(14);
  _worldList.resetSelection();
  _pickList.setModel(&_pickModel);
  _pickList.setRowHeight(14);
  _pomoSetupList.setModel(&_pomoSetupModel);
  _pomoSetupList.setRowHeight(11);
  _pomoSetupList.resetSelection();
  _pomoSetupOpen = false;
  _pomoStepRow = -1;
}

int ClockApplet::onRender(Canvas& c) {
  _now = c.now();
  int w = c.width(), h = c.height();
  _tabs.setBattery(_app ? _app->batteryMillivolts() : 0);
  const int barH = barHeight(c, BAR_H);
  _tabs.draw(c, 0, 0, w, barH);
  int bodyY = barH + 1;
  int bodyH = h - bodyY;

  if (settingsTab()) return timeSettings().renderBody(c, 0, bodyY, w, bodyH);
  if (_pickingCity) {
    _pickList.draw(c, 0, bodyY, w, bodyH);
    return _pickList.needsAnimation() ? ListMenu::TICK_MS : 500;
  }
  // Setup is a full-body list, not an overlay: render it INSTEAD of the tab body
  // so the idle Ready screen does not show through behind the rows.
  if (_pomoSetupOpen) {
    _pomoSetupList.draw(c, 0, bodyY, w, bodyH);
    if (_pomoStepRow >= 0) _pomoStepper.draw(c, 0, 0, w, h);
    return _pomoStepRow >= 0 ? 100 : (_pomoSetupList.needsAnimation() ? ListMenu::TICK_MS : 500);
  }

  int next;
  switch (_tab) {
    case TAB_STOPWATCH: next = renderStopwatch(c, bodyY, bodyH); break;
    case TAB_TIMER:     next = renderTimer(c, bodyY, bodyH); break;
    case TAB_POMODORO:
      next = clockService().pmActive() ? renderPomodoroRunning(c, bodyY, bodyH)
                                       : renderPomodoroIdle(c, bodyY, bodyH);
      break;
    case TAB_ALARM:     next = renderAlarm(c, bodyY, bodyH); break;
    default:            next = renderWorld(c, bodyY, bodyH); break;
  }
  if (_editor.open) { drawEditor(c); return 100; }
  return next;
}

// A magnified readout has to clear the width it is centred in, not just the
// panel height: on a 122x250 portrait canvas the height says "roomy" while the
// scale-2 digits run off both edges.
static int numScale(Canvas& c, const char* str, int maxW) {
  return c.fitScale(fontNum(), str, maxW, numScaleCap(c));
}
static const Font* hintFont(Canvas& c) { return tierFont(c); }

int ClockApplet::renderStopwatch(Canvas& c, int y, int h) {
  ClockService& svc = clockService();
  uint32_t ms = svc.swElapsedMs(_now);
  char buf[16];
  fmtStopwatch(buf, sizeof(buf), ms);
  const Font* hf = hintFont(c);
  int capH = c.lineHeight(hf);
  int lapH = c.lineHeight(fontCaption());   // the lap list stays in the dense tier
  int avail = h - capH - 2;               // body above the hint line
  int laps = svc.swLapCount();
  // Laps stack under the readout on a portrait panel: the 40px column they take
  // in landscape is a third of a 122px width, and the readout loses a whole
  // magnification step to pay for it.
  const bool sideLaps = laps > 0 && !isPortrait(c);
  const int lapW = 40;                    // "8 88:88.8" in the caption tier
  int numW = sideLaps ? c.width() - lapW : c.width();
  const int ns = numScale(c, buf, numW);
  int nh = c.fontHeightScaled(fontNum(), ns);
  int ny = y + (avail - nh) / 2;          // vertically centred readout
  if (laps == 0) {
    c.drawTextScaled(fontNum(), c.width() / 2, ny, buf, DisplayDriver::LIGHT, ns, TextAlign::Center);
  } else if (!sideLaps) {
    ny = y + 1;                           // readout on top, laps fill below it
    c.drawTextScaled(fontNum(), c.width() / 2, ny, buf, DisplayDriver::LIGHT, ns, TextAlign::Center);
    int ly = ny + nh + 3;
    int maxRows = (y + avail - ly) / lapH;
    for (int i = 0; i < laps && i < maxRows; i++) {
      char lap[16], line[20];
      fmtStopwatch(lap, sizeof(lap), svc.swLapMs(i));
      snprintf(line, sizeof(line), "%d %s", svc.swLapNumber(i), lap);
      c.drawText(fontCaption(), 2, ly, line, DisplayDriver::LIGHT);
      ly += lapH;
    }
  } else {
    // Two columns: readout centred in the left column, lap list (newest first)
    // down the right edge - fits several laps instead of two.
    int lx = c.width() - lapW;
    c.drawTextScaled(fontNum(), lx / 2, ny, buf, DisplayDriver::LIGHT, ns, TextAlign::Center);
    int maxRows = (avail - 1) / lapH;
    int ly = y + 1;
    for (int i = 0; i < laps && i < maxRows; i++) {
      char lap[16], line[20];
      fmtStopwatch(lap, sizeof(lap), svc.swLapMs(i));
      snprintf(line, sizeof(line), "%d %s", svc.swLapNumber(i), lap);
      c.drawText(fontCaption(), lx, ly, line, DisplayDriver::LIGHT);
      ly += lapH;
    }
  }

  const char* hint = svc.swRunning() ? "Sel stop / up lap / hold reset"
                    : ms ? "Sel resume / hold reset" : "Select to start";
  c.drawText(hf, c.width() / 2, y + h - capH, hint,
             DisplayDriver::LIGHT, TextAlign::Center);
  return svc.swRunning() ? 50 : 60000;
}

int ClockApplet::renderTimer(Canvas& c, int y, int h) {
  ClockService& svc = clockService();
  char buf[16];
  fmtCountdown(buf, sizeof(buf), svc.tmRemainingMs(_now));
  const int ns = numScale(c, buf, c.width());
  const Font* hf = hintFont(c);
  int nh = c.fontHeightScaled(fontNum(), ns);
  int capH = c.lineHeight(hf);
  int avail = h - capH - 2;
  bool paused = svc.tmPaused();
  int block = paused ? nh + 2 + capH : nh;   // centre readout (+ "Paused") as one block
  int ny = y + (avail - block) / 2;
  c.drawTextScaled(fontNum(), c.width() / 2, ny, buf, DisplayDriver::LIGHT, ns, TextAlign::Center);
  if (paused)
    c.drawText(hf, c.width() / 2, ny + nh + 2, "Paused",
               DisplayDriver::LIGHT, TextAlign::Center);

  const char* hint = svc.tmRunning() ? "Sel pause / hold reset"
                    : paused ? "Sel resume / hold reset"
                             : "Up/down set / sel start";
  c.drawText(hf, c.width() / 2, y + h - capH, hint,
             DisplayDriver::LIGHT, TextAlign::Center);
  return svc.tmRunning() ? 200 : 60000;
}

int ClockApplet::renderAlarm(Canvas& c, int y, int h) {
  int listH = 2 * _alarmList.rowHeight() + 2;
  _alarmList.draw(c, 0, y, c.width(), listH < h ? listH : h);

  char buf[28] = "";
  if (clockService().alarmEnabled() && _app) {
    int32_t mins = clockService().alarmMinutesAway(_app->epochSeconds(), _app->tzOffsetMinutes());
    if (mins < 0) snprintf(buf, sizeof(buf), "Clock not set");
    else if (mins == 0) snprintf(buf, sizeof(buf), "Rings now");
    else if (mins >= 60) snprintf(buf, sizeof(buf), "Rings in %ldh %02ldm", (long)(mins / 60), (long)(mins % 60));
    else snprintf(buf, sizeof(buf), "Rings in %ldm", (long)mins);
  }
  if (buf[0])
    c.drawText(fontCaption(), c.width() / 2, y + h - c.lineHeight(fontCaption()), buf,
               DisplayDriver::LIGHT, TextAlign::Center);
  return _alarmList.needsAnimation() ? ListMenu::TICK_MS
         : clockService().alarmEnabled() ? 15000 : 60000;
}

int ClockApplet::renderWorld(Canvas& c, int y, int h) {
  _worldList.draw(c, 0, y, c.width(), h);
  return _worldList.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

// Pomodoro geometry. The ring hugs the left edge and the phase word + readout sit
// centred in whatever column is left, so the pair stays put as the canvas widens
// instead of clinging to coordinates picked for a 128px screen. Idle and running
// both go through this: starting a session must not shift the text.
struct PomoLayout {
  int cx, cy, r;      // ring
  int tx, labelY, numY;
  int hintH;
  int ns;             // readout magnification, already fitted to its column
};

// `readout` is the widest string the caller will draw at p.numY - the column
// width and the magnification are decided together, so a longer countdown
// cannot overrun the ring on one side and the panel edge on the other.
static PomoLayout pomoLayout(Canvas& c, int y, int h, const char* readout) {
  PomoLayout p;
  p.r = isRegularCanvas(c) ? 26 : 16;   // hold its own against a magnified readout
  p.hintH = c.lineHeight(hintFont(c));
  const int labelH = c.lineHeight(fontCaption());
  const int band = h - p.hintH;         // everything above the hint line

  if (isPortrait(c)) {
    // Ring over readout. Side by side, the ring eats half of a 122px width and
    // the digits are left overlapping it on one edge and clipped on the other.
    p.cx = c.width() / 2;
    p.tx = c.width() / 2;
    p.ns = c.fitScale(fontNum(), readout, c.width() - 4, numScaleCap(c));
    const int numH = c.fontHeightScaled(fontNum(), p.ns);
    const int blockH = 2 * p.r + 6 + labelH + 2 + numH;
    const int top = y + (band - blockH) / 2;
    p.cy = top + p.r;
    p.labelY = top + 2 * p.r + 6;
    p.numY = p.labelY + labelH + 2;
    return p;
  }

  p.cx = 4 + p.r;
  p.cy = y + band / 2;
  const int colX = p.cx + p.r + 6;
  p.tx = colX + (c.width() - colX) / 2;
  p.ns = c.fitScale(fontNum(), readout, c.width() - colX, numScaleCap(c));
  const int numH = c.fontHeightScaled(fontNum(), p.ns);
  const int blockH = labelH + 2 + numH;
  p.labelY = y + (band - blockH) / 2;
  p.numY = p.labelY + labelH + 2;
  return p;
}

int ClockApplet::renderPomodoroIdle(Canvas& c, int y, int h) {
  ClockService& s = clockService();
  char buf[8]; snprintf(buf, sizeof(buf), "%u:00", s.pmFocusMin());
  const PomoLayout p = pomoLayout(c, y, h, buf);
  int n = s.pmSetCount(), seg = 360 / n, gap = 12;
  for (int i = 0; i < n; i++)
    c.drawArc(p.cx, p.cy, p.r, 1, i * seg + gap / 2, (i + 1) * seg - gap / 2,
              DisplayDriver::LIGHT);                 // faint track = the empty set
  c.drawGlyph(iconFont(), p.cx - 6, p.cy - 6, (uint16_t)Icon::Tomato, DisplayDriver::LIGHT);

  c.drawText(fontCaption(), p.tx, p.labelY, "FOCUS", DisplayDriver::LIGHT, TextAlign::Center);
  c.drawTextScaled(fontNum(), p.tx, p.numY, buf, DisplayDriver::LIGHT, p.ns, TextAlign::Center);
  c.drawText(hintFont(c), c.width() / 2, y + h - p.hintH,
             "Sel start / hold setup", DisplayDriver::LIGHT, TextAlign::Center);
  return 1000;
}

void ClockApplet::drawSessionRing(Canvas& c, int cx, int cy, int r) {
  ClockService& s = clockService();
  int n = s.pmSetCount(), seg = 360 / n, gap = 12;
  PomoPhase ph = s.pmPhase();
  int block = s.pmBlock();                       // 1..N
  int doneBlocks = (ph == PomoPhase::Focus) ? block - 1 : block;   // solid segments
  bool focusing = ph == PomoPhase::Focus;
  int activePct = focusing ? s.pmElapsedPct(_now) : 0;

  for (int i = 0; i < n; i++) {
    int a0 = i * seg + gap / 2, a1 = (i + 1) * seg - gap / 2;
    if (i < doneBlocks) {
      c.drawArc(cx, cy, r, 3, a0, a1, DisplayDriver::LIGHT);       // done: thick
    } else if (focusing && i == doneBlocks) {
      c.drawArc(cx, cy, r, 1, a0, a1, DisplayDriver::LIGHT);       // track under fill
      int fillEnd = a0 + (a1 - a0) * activePct / 100;
      if (fillEnd > a0) c.drawArc(cx, cy, r, 3, a0, fillEnd, DisplayDriver::LIGHT);
    } else {
      c.drawArc(cx, cy, r, 1, a0, a1, DisplayDriver::LIGHT);       // to-go: thin track
    }
  }
  Icon g = (ph == PomoPhase::Focus) ? Icon::Tomato : Icon::Coffee;
  c.drawGlyph(iconFont(), cx - 6, cy - 6, (uint16_t)g, DisplayDriver::LIGHT);
}

int ClockApplet::renderPomodoroRunning(Canvas& c, int y, int h) {
  ClockService& s = clockService();
  char buf[8];
  uint32_t sec = (s.pmRemainingMs(_now) + 999u) / 1000u;
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
  const PomoLayout p = pomoLayout(c, y, h, buf);
  drawSessionRing(c, p.cx, p.cy, p.r);

  const char* word;
  switch (s.pmPhase()) {
    case PomoPhase::ShortBreak: word = "BREAK"; break;
    case PomoPhase::LongBreak:  word = "LONG BREAK"; break;
    default:                    word = "FOCUS"; break;
  }
  c.drawText(fontCaption(), p.tx, p.labelY, word, DisplayDriver::LIGHT, TextAlign::Center);
  c.drawTextScaled(fontNum(), p.tx, p.numY, buf, DisplayDriver::LIGHT, p.ns, TextAlign::Center);

  const char* hint = s.pmRunning() ? "Sel pause / hold reset"
                                    : "Sel start / hold reset";
  c.drawText(hintFont(c), c.width() / 2, y + h - p.hintH, hint,
             DisplayDriver::LIGHT, TextAlign::Center);
  return s.pmRunning() ? 250 : 60000;
}

void ClockApplet::openPomodoroSetup() {
  _pomoSetupOpen = true;
  _pomoStepRow = -1;
  _pomoSetupList.resetSelection();
}

void ClockApplet::openAlarmEditor() {
  int h24 = clockService().alarmHour();
  _editor = TimeEditor{};
  _editor.title = "Alarm time";
  _editor.vals[1] = clockService().alarmMinute();
  _editor.maxs[1] = 60;
  if (_app && _app->timeFormat12h()) {   // hour shown 1..12 + an AM/PM field
    _editor.nFields = 3;
    _editor.vals[0] = h24 % 12 ? h24 % 12 : 12;
    _editor.mins[0] = 1;
    _editor.maxs[0] = 13;
    _editor.vals[2] = h24 >= 12 ? 1 : 0;
    _editor.maxs[2] = 2;
    _editor.ampmField = 2;
  } else {
    _editor.nFields = 2;
    _editor.vals[0] = h24;
    _editor.maxs[0] = 24;
  }
  _editor.open = true;
}

void ClockApplet::openTimerEditor() {
  uint32_t s = clockService().tmDurationSecs();
  _editor = TimeEditor{};
  _editor.title = "Timer";
  _editor.nFields = 3;
  _editor.vals[0] = (int)(s / 3600);
  _editor.vals[1] = (int)((s / 60) % 60);
  _editor.vals[2] = (int)(s % 60);
  _editor.maxs[0] = (int)(ClockService::TIMER_MAX_SECS / 3600) + 1;
  _editor.maxs[1] = 60;
  _editor.maxs[2] = 60;
  _editor.sel = 1;   // minutes is the field people usually mean
  _editor.forTimer = true;
  _editor.open = true;
}

void ClockApplet::drawEditor(Canvas& c) {
  // Title line over one row of digits - the frame has no reason to be taller.
  Canvas box = drawModalChrome(c, 0, 3 + c.lineHeight(fontBody()) + 4
                                       + c.fontHeight(fontNum()) + 3 + 4);
  int w = box.width();
  box.drawText(fontBody(), w / 2, 3, _editor.title, DisplayDriver::LIGHT, TextAlign::Center);

  int nh = c.fontHeight(fontNum());
  int ty = 3 + c.lineHeight(fontBody()) + 4;
  int colonW = box.textWidth(fontNum(), ":");
  int digitW = box.textWidth(fontNum(), "00");
  int ampmW = box.textWidth(fontSubtitle(), "PM");   // fontNum has no letters
  int total = 0;
  for (int i = 0; i < _editor.nFields; i++) {
    total += i == _editor.ampmField ? ampmW : digitW;
    if (i < _editor.nFields - 1) total += (i + 1 == _editor.ampmField) ? 6 : colonW + 4;
  }
  int x = (w - total) / 2;
  for (int i = 0; i < _editor.nFields; i++) {
    int fw;
    if (i == _editor.ampmField) {
      fw = ampmW;
      box.drawText(fontSubtitle(), x, ty + nh - c.fontHeight(fontSubtitle()),
                   _editor.vals[i] ? "PM" : "AM", DisplayDriver::LIGHT);
    } else {
      fw = digitW;
      char v[4];
      snprintf(v, sizeof(v), "%02u", (unsigned)_editor.vals[i]);
      box.drawText(fontNum(), x, ty, v, DisplayDriver::LIGHT);
    }
    if (i == _editor.sel)
      box.fillRect(x, ty + nh + 1, fw, 2, DisplayDriver::LIGHT);   // active field
    x += fw;
    if (i < _editor.nFields - 1) {
      if (i + 1 == _editor.ampmField) x += 6;
      else {
        box.drawText(fontNum(), x + 2, ty, ":", DisplayDriver::LIGHT);
        x += colonW + 4;
      }
    }
  }
  box.drawText(fontCaption(), w / 2, box.height() - c.lineHeight(fontCaption()) - 2,
               "Select to save", DisplayDriver::LIGHT, TextAlign::Center);
}

bool ClockApplet::inputEditor(InputEvent ev) {
  TimeEditor& e = _editor;
  if (ev == InputEvent::NavLeft || ev == InputEvent::NavRight) {
    int d = ev == InputEvent::NavRight ? 1 : -1;
    e.sel = (e.sel + e.nFields + d) % e.nFields;
  } else if (ev == InputEvent::NavUp || ev == InputEvent::NavDown) {
    int d = ev == InputEvent::NavUp ? 1 : -1;
    int range = e.maxs[e.sel] - e.mins[e.sel];
    e.vals[e.sel] = e.mins[e.sel] + (e.vals[e.sel] - e.mins[e.sel] + range + d) % range;
  } else if (ev == InputEvent::Select) {
    if (e.forTimer) {
      clockService().tmSetDurationSecs(
          (uint32_t)e.vals[0] * 3600u + (uint32_t)e.vals[1] * 60u + (uint32_t)e.vals[2]);
    } else {
      int h24 = e.ampmField >= 0 ? (e.vals[0] % 12) + (e.vals[e.ampmField] ? 12 : 0)
                                 : e.vals[0];
      // Saving a time is arming it: enable in the same step.
      clockService().setAlarm((uint8_t)h24, (uint8_t)e.vals[1], true,
                              _app ? _app->epochSeconds() : 0,
                              _app ? _app->tzOffsetMinutes() : 0);
      if (_host) _host->postToast("Alarm set");
    }
    e.open = false;
  } else if (ev == InputEvent::Back || ev == InputEvent::Cancel) {
    e.open = false;
  }
  return true;   // swallow everything while modal
}

bool ClockApplet::onInput(InputEvent ev) {
  // Settings tab: delegate entirely to the shared panel.
  if (settingsTab()) {
    if (timeSettings().modalActive()) return timeSettings().onInput(ev);
    if (_tabs.onInput(ev)) { _tab = _tabs.selected(); return true; }
    if (timeSettings().onInput(ev)) return true;
    return false;   // Back bubbles
  }

  if (_editor.open) return inputEditor(ev);
  if (_pickingCity) return inputWorld(ev);
  // Setup overlay owns input; the tab strip must not steal NavLeft/NavRight from it.
  if (_tab == TAB_POMODORO && _pomoSetupOpen) return inputPomodoro(ev);

  if (_tabs.onInput(ev)) {
    _tab = _tabs.selected();
    return true;
  }
  switch (_tab) {
    case TAB_STOPWATCH: return inputStopwatch(ev);
    case TAB_TIMER:     return inputTimer(ev);
    case TAB_POMODORO:  return inputPomodoro(ev);
    case TAB_ALARM:     return inputAlarm(ev);
    default:            return inputWorld(ev);
  }
}

bool ClockApplet::inputStopwatch(InputEvent ev) {
  ClockService& svc = clockService();
  if (ev == InputEvent::Select) { svc.swToggle(_now); return true; }
  if (ev == InputEvent::SelectLong) { svc.swReset(); return true; }
  if (ev == InputEvent::NavUp && svc.swRunning()) { svc.swLap(_now); return true; }
  return false;
}

bool ClockApplet::inputTimer(InputEvent ev) {
  ClockService& svc = clockService();
  if (ev == InputEvent::Select) { svc.tmToggle(_now); return true; }
  if (ev == InputEvent::SelectLong) { svc.tmReset(); return true; }
  if ((ev == InputEvent::NavUp || ev == InputEvent::NavDown) && !svc.tmRunning() && !svc.tmPaused()) {
    openTimerEditor();
    return true;
  }
  return false;
}

bool ClockApplet::inputAlarm(InputEvent ev) {
  ClockService& svc = clockService();
  if (_alarmList.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    if (_alarmList.selected() == AlarmModel::Time) {
      openAlarmEditor();
    } else {
      svc.setAlarm(svc.alarmHour(), svc.alarmMinute(), !svc.alarmEnabled(),
                   _app ? _app->epochSeconds() : 0,
                   _app ? _app->tzOffsetMinutes() : 0);
    }
    return true;
  }
  return false;
}

bool ClockApplet::inputWorld(InputEvent ev) {
  ClockService& svc = clockService();
  if (_pickingCity) {
    if (_pickList.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      if (!svc.addCity((uint8_t)_pickList.selected()) && _host)
        _host->postToast("Already added");
      _pickingCity = false;
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _pickingCity = false; return true; }
    return true;   // swallow everything while picking
  }

  if (_worldList.onInput(ev)) return true;
  int sel = _worldList.selected();
  if (ev == InputEvent::Select) {
    if (sel >= svc.cityCount()) {   // "Add city" row
      _pickList.resetSelection();
      _pickingCity = true;
    }
    return true;
  }
  if (ev == InputEvent::SelectLong && sel < svc.cityCount()) {
    svc.removeCity(sel);
    if (_worldList.selected() >= _worldModel.count())
      _worldList.setSelected(_worldModel.count() - 1);
    if (_host) _host->postToast("City removed");
    return true;
  }
  return false;
}

bool ClockApplet::inputPomodoro(InputEvent ev) {
  ClockService& s = clockService();

  // Setup overlay owns input while open.
  if (_pomoSetupOpen) {
    if (_pomoStepRow >= 0) {                 // stepper modal active
      _pomoStepper.onInput(ev);
      StepperResult res = _pomoStepper.result();
      if (res == StepperResult::Confirmed) {
        int v = _pomoStepper.value();
        switch (_pomoStepRow) {
          case PomoSetupModel::Focus: s.setPmFocusMin((uint8_t)v); break;
          case PomoSetupModel::Short: s.setPmShortMin((uint8_t)v); break;
          case PomoSetupModel::Long:  s.setPmLongMin((uint8_t)v); break;
          case PomoSetupModel::SetN:  s.setPmSetCount((uint8_t)v); break;
        }
      }
      if (res != StepperResult::None) { _pomoStepper.reset(); _pomoStepRow = -1; }
      return true;
    }
    if (_pomoSetupList.onInput(ev)) return true;
    int row = _pomoSetupList.selected();
    if (ev == InputEvent::Select) {
      if (row == PomoSetupModel::Auto) { s.setPmAutoAdvance(!s.pmAutoAdvance()); return true; }
      switch (row) {
        case PomoSetupModel::Focus: _pomoStepper.configure("Focus min", s.pmFocusMin(), 5, 60); break;
        case PomoSetupModel::Short: _pomoStepper.configure("Short break min", s.pmShortMin(), 1, 30); break;
        case PomoSetupModel::Long:  _pomoStepper.configure("Long break min", s.pmLongMin(), 5, 45); break;
        case PomoSetupModel::SetN:  _pomoStepper.configure("Blocks per set", s.pmSetCount(), 2, 8); break;
      }
      _pomoStepRow = row;
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _pomoSetupOpen = false; return true; }
    return true;   // swallow everything while in setup
  }

  // Idle: start, or open setup.
  if (!s.pmActive()) {
    if (ev == InputEvent::Select) { s.pmStart(_now); return true; }
    if (ev == InputEvent::SelectLong) { openPomodoroSetup(); return true; }
    return false;
  }
  if (ev == InputEvent::Select) { s.pmToggle(_now); return true; }
  if (ev == InputEvent::SelectLong) { s.pmReset(); return true; }
  return false;
}

ClockApplet& clockApplet() {
  static ClockApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&clockApplet(), Placement::AppMenu, "Clock", 5,
                              (uint16_t)Icon::Clock);

}  // namespace mishmesh
