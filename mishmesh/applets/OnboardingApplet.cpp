#include <mishmesh/applets/OnboardingApplet.h>
#include <mishmesh/applets/onboarding_logo.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/applets/SetTimeApplet.h>
#include <mishmesh/applets/TimezonePickerApplet.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/NameValidation.h>
#include <mishmesh/core/WorldClock.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

bool shouldShowOnboarding(uint8_t state, bool freshIdentity) {
  if (state == 2) return false;   // DONE
  if (state == 1) return true;    // IN_PROGRESS
  return freshIdentity;           // NOT_STARTED
}

// ---- step machine ---------------------------------------------------------

void OnboardingApplet::buildSteps(bool gps) {
  _stepCount = 0;
  _steps[_stepCount++] = Welcome;
  _steps[_stepCount++] = Name;
  _steps[_stepCount++] = Region;
  if (gps) _steps[_stepCount++] = Gps;
  _steps[_stepCount++] = Time;
  _steps[_stepCount++] = Done;
}

const char* OnboardingApplet::stepTitle() const {
  switch (cur()) {
    case Welcome: return "Welcome";
    case Name:    return "Device name";
    case Region:  return "Radio region";
    case Gps:     return "GPS";
    case Time:    return "Time";
    case Done:    return "All set";
    default:      return "Setup";
  }
}

void OnboardingApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _idx = 0;
  buildSteps(_app && _app->gpsSupported());
  if (_app) {
    snprintf(_name, sizeof(_name), "%s", _app->nodeName() ? _app->nodeName() : "");
    _gps = _app->gpsEnabled();
    _tzCity = _app->tzCityIndex();
    _autoSync = _app->autoTimeSync();
    RadioConfig cfg{};
    if (_app->radioConfig(cfg)) {
      int m = matchPreset(cfg.freqMhz, cfg.bwKhz, cfg.sf, cfg.cr);
      if (m >= 0) _region = m;
    }
  }
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
}

void OnboardingApplet::enterStep() {
  _list.resetSelection();
  if (cur() == Welcome) _logoSnap = true;   // replay the slide-up on (re)entry
  // Radio region: start focus on the currently-chosen preset so it's visible.
  if (cur() == Region && _region >= 0 && _region < PRESET_COUNT) _list.setSelected(_region);
}
void OnboardingApplet::advance() { if (_idx < _stepCount - 1) { _idx++; enterStep(); } }
void OnboardingApplet::back()    { if (_idx > 0) { _idx--; enterStep(); } }

void OnboardingApplet::onNameDone(void* ctx, const char* text) {
  OnboardingApplet* self = (OnboardingApplet*)ctx;
  if (self && text) snprintf(self->_name, sizeof(self->_name), "%s", text);
}
void OnboardingApplet::onTzPicked(void* ctx, int cityIndex) {
  OnboardingApplet* self = (OnboardingApplet*)ctx;
  if (self) self->_tzCity = cityIndex;
}

void OnboardingApplet::activate(int row) {
  switch (cur()) {
    case Name:
      if (row == 0) {
        keypadApplet().configure(_name, sizeof(_name) - 1, "Device name", &OnboardingApplet::onNameDone, this);
        if (_host) _host->push(&keypadApplet());
      } else advance();
      break;
    case Region:
      if (row >= 0 && row < PRESET_COUNT) { _region = row; advance(); }   // pick + auto-advance
      break;
    case Gps:
      if (row == 0) _gps = !_gps;
      else advance();
      break;
    case Time:
      if (row == 0) {
        timezonePickerApplet().configure(_tzCity, &OnboardingApplet::onTzPicked, this);
        if (_host) _host->push(&timezonePickerApplet());
      } else if (row == 1) {
        _autoSync = !_autoSync;                          // row count changes (3<->4)
        if (_list.selected() >= count()) _list.setSelected(count() - 1);
      } else if (!_autoSync && row == 2) {
        setTimeApplet().configure(_app);                 // manual clock, applies immediately
        if (_host) _host->push(&setTimeApplet());
      } else advance();                                  // Next
      break;
    default: break;
  }
}

// Apply only what changed, so Finish does the minimum flash writes / radio reinit.
void OnboardingApplet::finish() {
  if (_app) {
    const char* curName = _app->nodeName() ? _app->nodeName() : "";
    if (isValidNodeName(_name) && strcmp(_name, curName) != 0) _app->setNodeName(_name);

    RadioConfig cfg{};
    bool haveCfg = _app->radioConfig(cfg);
    if (_region >= 0 && _region < PRESET_COUNT) {
      int curPreset = haveCfg ? matchPreset(cfg.freqMhz, cfg.bwKhz, cfg.sf, cfg.cr) : -1;
      if (curPreset != _region) {                        // only reconfigure the radio if the band changed
        const RadioPreset& p = PRESETS[_region];
        if (!haveCfg) cfg = RadioConfig{};
        cfg.freqMhz = p.freq; cfg.bwKhz = p.bw; cfg.sf = p.sf; cfg.cr = p.cr;
        _app->setRadioConfig(cfg);                       // txPowerDbm/repeater preserved
      }
    }
    if (_app->gpsSupported() && _gps != _app->gpsEnabled()) _app->setGpsEnabled(_gps);
    if (_tzCity >= 0 && _tzCity != _app->tzCityIndex())     _app->setTzCity(_tzCity);
    if (_autoSync != _app->autoTimeSync())                  _app->setAutoTimeSync(_autoSync);
    _app->markOnboardingComplete();
  }
  if (_done) _done(_doneCtx);   // adapter persists + replace(home)
}

// ---- input ---------------------------------------------------------------

bool OnboardingApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Back) { back(); return true; }   // Welcome: no-op (can't exit)

  Step s = cur();
  if (s == Welcome) { if (ev == InputEvent::Select) advance(); return true; }
  if (s == Done)    { if (ev == InputEvent::Select) finish(); return true; }

  if (_list.onInput(ev)) return true;                    // NavUp/NavDown move focus
  if (ev == InputEvent::Select) { activate(_list.selected()); return true; }
  return true;                                           // modal root: never bubble
}

// ---- ListModel (middle steps) --------------------------------------------

int OnboardingApplet::count() const {
  switch (cur()) {
    case Name:   return 2;
    case Region: return PRESET_COUNT;
    case Gps:    return 2;
    case Time:   return _autoSync ? 3 : 4;
    default:     return 0;
  }
}

const char* OnboardingApplet::label(int i) const {
  switch (cur()) {
    case Name:   return i == 0 ? "Name" : "Next";
    case Region: return (i >= 0 && i < PRESET_COUNT) ? PRESETS[i].name : "";
    case Gps:    return i == 0 ? "Enable GPS" : "Next";
    case Time:
      if (i == 0) return "Time zone";
      if (i == 1) return "Auto-sync";
      if (!_autoSync && i == 2) return "Set clock...";
      return "Next";
    default: return "";
  }
}

const char* OnboardingApplet::value(int i) const {
  if (cur() == Name && i == 0) return _name[0] ? _name : "(unset)";
  if (cur() == Time && i == 0) {
    if (_tzCity >= 0 && _tzCity < worldCityCount()) return worldCity(_tzCity).name;
    return "(set)";
  }
  return nullptr;
}

bool OnboardingApplet::isToggle(int i) const {
  return (cur() == Gps && i == 0) || (cur() == Time && i == 1);
}
bool OnboardingApplet::toggleState(int i) const {
  if (cur() == Gps && i == 0) return _gps;
  if (cur() == Time && i == 1) return _autoSync;
  return false;
}
bool OnboardingApplet::isRadio(int i) const { return cur() == Region && i >= 0 && i < PRESET_COUNT; }
bool OnboardingApplet::radioOn(int i) const { return cur() == Region && i == _region; }
bool OnboardingApplet::isButton(int i) const { return hasNextButton(cur()) && i == count() - 1; }

// ---- render --------------------------------------------------------------

void OnboardingApplet::drawTitleBar(Canvas& c, int w) {
  c.drawText(fontBody(), 2, 1, stepTitle(), DisplayDriver::LIGHT);
  // Welcome is an uncounted cover screen (no title bar), so counting starts at Name:
  // _idx is already the 1-based position among the counted steps, over _stepCount-1 total.
  char cnt[8]; snprintf(cnt, sizeof(cnt), "%d/%d", _idx, _stepCount - 1);
  c.drawText(fontBody(), w - 2, 1, cnt, DisplayDriver::LIGHT, TextAlign::Right);
}

void OnboardingApplet::drawCenterButton(Canvas& c, int x, int y, int w, const char* text) {
  int bh = c.fontHeight(fontBody()) + 4;
  int bw = c.textWidth(fontBody(), text) + 16;
  if (bw > w - 8) bw = w - 8;
  _btn.set(text, 0);
  _btn.setFocused(true);
  _btn.draw(c, x + (w - bw) / 2, y, bw, bh);
}

void OnboardingApplet::drawWelcome(Canvas& c, int x, int y, int w, int h) {
  const int gap = 5;
  const int blockH = MESHCORE_LOGO_H + gap + MISHMESH_LOGO_H;
  const int finalTop = y + 6;
  const int splashTop = y + (h - blockH) / 2;  // where UITask::drawBootSplash left them

  if (_logoSnap) { _logoTop = splashTop; _logoSnap = false; }
  _logoTop = approach(_logoTop, finalTop, 1);
  _logoSettling = (_logoTop != finalTop);

  c.drawXbm(x + (w - MESHCORE_LOGO_W) / 2, _logoTop, MESHCORE_LOGO, MESHCORE_LOGO_W, MESHCORE_LOGO_H);
  c.drawXbm(x + (w - MISHMESH_LOGO_W) / 2, _logoTop + MESHCORE_LOGO_H + gap,
            MISHMESH_LOGO, MISHMESH_LOGO_W, MISHMESH_LOGO_H);

  // hold the button back until the logos land, so the first frame matches the splash
  if (!_logoSettling)
    drawCenterButton(c, x, y + h - (c.fontHeight(fontBody()) + 5), w, "Get started");
}

void OnboardingApplet::drawDone(Canvas& c, int x, int y, int w, int h) {
  c.drawTextEllipsized(fontSubtitle(), x + w / 2, y + 2, w - 6, "You're all set",
                       DisplayDriver::LIGHT, TextAlign::Center);
  char line[40];
  const char* region = (_region >= 0 && _region < PRESET_COUNT) ? PRESETS[_region].name : "?";
  snprintf(line, sizeof(line), "%s / %s", _name[0] ? _name : "(unset)", region);
  c.drawTextEllipsized(fontBody(), x + w / 2, y + 20, w - 6, line, DisplayDriver::LIGHT, TextAlign::Center);
  drawCenterButton(c, x, y + h - (c.fontHeight(fontBody()) + 5), w, "Finish");
}

int OnboardingApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  c.fillRect(0, 0, w, h, DisplayDriver::DARK);

  Step s = cur();
  int by = 0, bhBody = h;
  if (s != Welcome) {                       // Welcome is full-screen branding: no title bar
    int bh = c.fontHeight(fontBody()) + 3;
    drawTitleBar(c, w);
    c.fillRect(0, bh - 1, w, 1, DisplayDriver::LIGHT);
    by = bh; bhBody = h - by;
  }

  if (s == Welcome)      drawWelcome(c, 0, by, w, bhBody);
  else if (s == Done)    drawDone(c, 0, by, w, bhBody);
  else                   _list.draw(c, 0, by, w, bhBody);

  if (s == Welcome && _logoSettling) return ListMenu::TICK_MS;
  return _list.needsAnimation() ? ListMenu::TICK_MS : 750;
}

}  // namespace mishmesh
