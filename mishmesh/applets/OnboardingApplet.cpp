#include <mishmesh/applets/OnboardingApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/applets/SetTimeApplet.h>
#include <mishmesh/applets/TimezonePickerApplet.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/NameValidation.h>
#include <mishmesh/core/WorldClock.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

bool shouldShowOnboarding(uint8_t state, bool freshIdentity) {
  if (state == 2) return false;   // DONE
  if (state == 1) return true;    // IN_PROGRESS
  return freshIdentity;           // NOT_STARTED
}

// Region list model over the shared PRESETS table (name only).
struct RegionModel : ListModel {
  int count() const override { return PRESET_COUNT; }
  const char* label(int i) const override {
    return (i >= 0 && i < PRESET_COUNT) ? PRESETS[i].name : "";
  }
};
static RegionModel s_regionModel;

void OnboardingApplet::buildSteps(bool gps) {
  _stepCount = 0;
  _steps[_stepCount++] = Welcome;
  _steps[_stepCount++] = Name;
  _steps[_stepCount++] = Region;
  if (gps) _steps[_stepCount++] = Gps;
  _steps[_stepCount++] = Time;
  _steps[_stepCount++] = Done;
}

void OnboardingApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _idx = 0;
  _timeRow = 0;
  buildSteps(_app && _app->gpsSupported());
  // seed staged state from current values
  if (_app) {
    snprintf(_name, sizeof(_name), "%s", _app->nodeName() ? _app->nodeName() : "");
    _gps = _app->gpsEnabled();
    _tzCity = _app->tzCityIndex();
    _autoSync = _app->autoTimeSync();
  }
  _regionList.setRowHeight(14);
  _regionList.setModel(&s_regionModel);
  _regionList.resetSelection();
  // Pre-select the preset matching the current radio so tapping through Region
  // without scrolling keeps the existing band (not blindly PRESETS[0]/Australia).
  if (_app) {
    RadioConfig cfg{};
    if (_app->radioConfig(cfg)) {
      int m = matchPreset(cfg.freqMhz, cfg.bwKhz, cfg.sf, cfg.cr);
      if (m >= 0) _regionList.setSelected(m);
    }
  }
}

void OnboardingApplet::onForeground() {
  // returning from a pushed child (keypad/picker/clock); nothing to reload here
  // beyond what the seams already staged via their callbacks.
}

void OnboardingApplet::onNameDone(void* ctx, const char* text) {
  OnboardingApplet* self = (OnboardingApplet*)ctx;
  if (self && text) snprintf(self->_name, sizeof(self->_name), "%s", text);
}

void OnboardingApplet::onTzPicked(void* ctx, int cityIndex) {
  OnboardingApplet* self = (OnboardingApplet*)ctx;
  if (self) self->_tzCity = cityIndex;
}

void OnboardingApplet::next() {
  if (_idx < _stepCount - 1) _idx++;
}
void OnboardingApplet::back() {
  if (_idx > 0) _idx--;
}

void OnboardingApplet::finish() {
  if (_app) {
    if (isValidNodeName(_name)) _app->setNodeName(_name);
    RadioConfig cfg{};
    if (!_app->radioConfig(cfg)) cfg = RadioConfig{};
    int r = _regionList.selected();
    if (r >= 0 && r < PRESET_COUNT) {
      const RadioPreset& p = PRESETS[r];
      cfg.freqMhz = p.freq; cfg.bwKhz = p.bw; cfg.sf = p.sf; cfg.cr = p.cr;
      _app->setRadioConfig(cfg);   // txPowerDbm/repeater preserved from the current config above
    }
    if (_app->gpsSupported()) _app->setGpsEnabled(_gps);
    if (_tzCity >= 0) _app->setTzCity(_tzCity);
    _app->setAutoTimeSync(_autoSync);
    _app->markOnboardingComplete();
  }
  if (_done) _done(_doneCtx);   // adapter persists + setRoot(home)
}

int OnboardingApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  c.fillRect(0, 0, w, h, DisplayDriver::DARK);
  const int titleY = 4, bodyY = h / 2 - 6, footY = h - 11;
  char line[40];

  switch (cur()) {
    case Welcome:
      c.drawGlyph(iconFont(), w / 2 - 6, titleY + 2, (uint16_t)Icon::Check, DisplayDriver::LIGHT);
      c.drawTextCentered(fontSubtitle(), 0, titleY + 16, w, 12, "Welcome to MeshCore", DisplayDriver::LIGHT);
      c.drawTextCentered(fontBody(), 0, titleY + 30, w, 10, "Your node is ready", DisplayDriver::LIGHT);
      { char pk[20]; if (_app) _app->selfPublicKeyHex(pk, sizeof(pk), 4); else pk[0] = 0;
        c.drawTextCentered(fontBody(), 0, titleY + 42, w, 10, pk, DisplayDriver::LIGHT); }
      c.drawTextCentered(fontBody(), 0, footY, w, 10, "Get started >", DisplayDriver::LIGHT);
      break;
    case Name:
      c.drawTextCentered(fontSubtitle(), 0, titleY, w, 12, "Device name", DisplayDriver::LIGHT);
      c.drawTextCentered(fontSubtitle(), 0, bodyY, w, 12, _name[0] ? _name : "(unset)", DisplayDriver::LIGHT);
      c.drawTextCentered(fontBody(), 0, footY, w, 10, "< Back   Edit   Next >", DisplayDriver::LIGHT);
      break;
    case Region: {
      c.drawTextCentered(fontSubtitle(), 0, titleY, w, 12, "Region", DisplayDriver::LIGHT);
      const int barH = titleY + 14;
      _regionList.draw(c, 0, barH, w, h - barH - 11);
      c.drawTextCentered(fontBody(), 0, footY, w, 10, "< Back   Next >", DisplayDriver::LIGHT);
      break;
    }
    case Gps:
      c.drawTextCentered(fontSubtitle(), 0, titleY, w, 12, "GPS", DisplayDriver::LIGHT);
      snprintf(line, sizeof(line), "Enable GPS:  %s", _gps ? "On" : "Off");
      c.drawTextCentered(fontSubtitle(), 0, bodyY, w, 12, line, DisplayDriver::LIGHT);
      c.drawTextCentered(fontBody(), 0, bodyY + 14, w, 10, "sets location + time", DisplayDriver::LIGHT);
      c.drawTextCentered(fontBody(), 0, footY, w, 10, "< Back  Toggle  Next >", DisplayDriver::LIGHT);
      break;
    case Time: {
      c.drawTextCentered(fontSubtitle(), 0, titleY, w, 12, "Time", DisplayDriver::LIGHT);
      char zone[24];
      if (_tzCity >= 0 && _tzCity < worldCityCount())
        snprintf(zone, sizeof(zone), "%s", worldCity(_tzCity).name);
      else snprintf(zone, sizeof(zone), "(set)");
      snprintf(line, sizeof(line), "%c Zone: %s", _timeRow == 0 ? '>' : ' ', zone);
      c.drawText(fontBody(), 6, bodyY - 6, line, DisplayDriver::LIGHT);
      snprintf(line, sizeof(line), "%c Auto-sync: %s", _timeRow == 1 ? '>' : ' ', _autoSync ? "On" : "Off");
      c.drawText(fontBody(), 6, bodyY + 4, line, DisplayDriver::LIGHT);
      if (!_autoSync) {
        snprintf(line, sizeof(line), "%c Set clock...", _timeRow == 2 ? '>' : ' ');
        c.drawText(fontBody(), 6, bodyY + 14, line, DisplayDriver::LIGHT);
      }
      c.drawTextCentered(fontBody(), 0, footY, w, 10, "< Back   Next >", DisplayDriver::LIGHT);
      break;
    }
    case Done: {
      c.drawTextCentered(fontSubtitle(), 0, titleY, w, 12, "All set", DisplayDriver::LIGHT);
      int r = _regionList.selected();
      snprintf(line, sizeof(line), "%s / %s", _name[0] ? _name : "(unset)",
               (r >= 0 && r < PRESET_COUNT) ? PRESETS[r].name : "?");
      c.drawTextCentered(fontBody(), 0, bodyY, w, 10, line, DisplayDriver::LIGHT);
      if (_app && _app->gpsSupported()) {
        snprintf(line, sizeof(line), "GPS %s", _gps ? "on" : "off");
        c.drawTextCentered(fontBody(), 0, bodyY + 11, w, 10, line, DisplayDriver::LIGHT);
      }
      c.drawTextCentered(fontBody(), 0, footY, w, 10, "< Back   Finish", DisplayDriver::LIGHT);
      break;
    }
    default: break;
  }
  return _regionList.needsAnimation() ? ListMenu::TICK_MS : 750;
}

bool OnboardingApplet::onInput(InputEvent ev) {
  // Universal step nav: Back/NavLeft -> previous step; NavRight -> next step.
  if (ev == InputEvent::Back || ev == InputEvent::NavLeft) { back(); return true; }
  if (ev == InputEvent::NavRight) { next(); return true; }

  switch (cur()) {
    case Welcome:
      if (ev == InputEvent::Select) { next(); return true; }
      break;
    case Name:
      if (ev == InputEvent::Select) {
        keypadApplet().configure(_name, sizeof(_name) - 1, "Device name", &OnboardingApplet::onNameDone, this);
        if (_host) _host->push(&keypadApplet());
        return true;
      }
      break;
    case Region:
      if (_regionList.onInput(ev)) return true;   // NavUp/NavDown scroll presets
      if (ev == InputEvent::Select) { next(); return true; }
      break;
    case Gps:
      if (ev == InputEvent::Select || ev == InputEvent::NavUp || ev == InputEvent::NavDown) {
        _gps = !_gps; return true;
      }
      break;
    case Time: {
      int rows = _autoSync ? 2 : 3;
      if (ev == InputEvent::NavUp)   { _timeRow = (_timeRow + rows - 1) % rows; return true; }
      if (ev == InputEvent::NavDown) { _timeRow = (_timeRow + 1) % rows; return true; }
      if (ev == InputEvent::Select) {
        if (_timeRow == 0) {
          timezonePickerApplet().configure(_tzCity, &OnboardingApplet::onTzPicked, this);
          if (_host) _host->push(&timezonePickerApplet());
        } else if (_timeRow == 1) {
          _autoSync = !_autoSync;   // toggling to auto hides the "set clock" row; _timeRow stays valid (<=1)
        } else {   // _timeRow == 2, manual set (auto off) — applies immediately
          setTimeApplet().configure(_app);
          if (_host) _host->push(&setTimeApplet());
        }
        return true;
      }
      break;
    }
    case Done:
      if (ev == InputEvent::Select) { finish(); return true; }
      break;
    default: break;
  }
  return true;   // wizard is modal-root: never bubble (no exit until Finish)
}

}  // namespace mishmesh
