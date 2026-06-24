#include <mishmesh/applets/settings/RadioSettingsPanel.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/applets/RadioValuePickerApplet.h>
#include <mishmesh/applets/RadioPresetPickerApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/applets/RepeaterApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>
#include <stdlib.h>

namespace mishmesh {

const char* RadioSettingsPanel::Model::label(int i) const {
  switch (i) {
    case 0: return "Preset";
    case 1: return "Frequency";
    case 2: return "Bandwidth";
    case 3: return "Spreading factor";
    case 4: return "Coding rate";
    case 5: return "Transmit power";
    default: return "Repeater";
  }
}

const char* RadioSettingsPanel::Model::value(int i) const {
  if (!cfg) return "";
  switch (i) {
    case 0: {
      int m = matchPreset(cfg->freqMhz, cfg->bwKhz, cfg->sf, cfg->cr);
      return m >= 0 ? PRESETS[m].name : "Custom";
    }
    case 1: snprintf(vbuf, sizeof(vbuf), "%g MHz", cfg->freqMhz); return vbuf;
    case 2: snprintf(vbuf, sizeof(vbuf), "%g kHz", cfg->bwKhz);  return vbuf;
    case 3: snprintf(vbuf, sizeof(vbuf), "SF%u", (unsigned)cfg->sf); return vbuf;
    case 4: snprintf(vbuf, sizeof(vbuf), "CR%u", (unsigned)cfg->cr); return vbuf;
    case 5: snprintf(vbuf, sizeof(vbuf), "%d dBm", (int)cfg->txPowerDbm); return vbuf;
    default:   // Repeater: Off, or the active off-grid frequency
      if (cfg->repeater) { snprintf(vbuf, sizeof(vbuf), "%.3f MHz", cfg->freqMhz); return vbuf; }
      return "Off";
  }
}

void RadioSettingsPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  if (!_app || !_app->radioConfig(_staged)) {
    _staged = {915.0f, 250.0f, 10, 5, 20, false};   // fallback when the seam is unavailable
  }
  _dirty = false;
  _model.cfg = &_staged;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
}

void RadioSettingsPanel::onHide() {
  if (_dirty && _app) { _app->setRadioConfig(_staged); _dirty = false; }
}

int RadioSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

void RadioSettingsPanel::stageFrequency(float mhz) {
  if (mhz < 150.0f) mhz = 150.0f; else if (mhz > 2500.0f) mhz = 2500.0f;
  _staged.freqMhz = mhz; _dirty = true;
}
void RadioSettingsPanel::stageBandwidth(float khz) { _staged.bwKhz = khz; _dirty = true; }
void RadioSettingsPanel::stageSf(uint8_t sf) { _staged.sf = sf; _dirty = true; }
void RadioSettingsPanel::stageCr(uint8_t cr) { _staged.cr = cr; _dirty = true; }
void RadioSettingsPanel::stageTxPower(int dbm) {
  int8_t hi = _app ? _app->txPowerMax() : 22;
  if (dbm < -9) dbm = -9; else if (dbm > hi) dbm = hi;
  _staged.txPowerDbm = (int8_t)dbm; _dirty = true;
}
void RadioSettingsPanel::stageRepeater(bool on) { _staged.repeater = on; _dirty = true; }

void RadioSettingsPanel::stagePreset(int idx) {
  if (idx < 0 || idx >= PRESET_COUNT) return;
  const RadioPreset& p = PRESETS[idx];
  _staged.freqMhz = p.freq; _staged.bwKhz = p.bw; _staged.sf = p.sf; _staged.cr = p.cr;
  _dirty = true;   // TX power intentionally left untouched
}

void RadioSettingsPanel::onFreqDone(void* ctx, const char* text) {
  RadioSettingsPanel* self = (RadioSettingsPanel*)ctx;
  if (text && text[0]) self->stageFrequency((float)atof(text));
}
void RadioSettingsPanel::onTxDone(void* ctx, const char* text) {
  RadioSettingsPanel* self = (RadioSettingsPanel*)ctx;
  if (text && text[0]) self->stageTxPower(atoi(text));
}

void RadioSettingsPanel::editFrequency() {
  snprintf(_scratch, sizeof(_scratch), "%g", _staged.freqMhz);
  keypadApplet().configureNumeric(_scratch, sizeof(_scratch) - 1, "Frequency (MHz)",
                                  &RadioSettingsPanel::onFreqDone, this);
  if (_host) _host->push(&keypadApplet());
}
void RadioSettingsPanel::editTxPower() {
  snprintf(_scratch, sizeof(_scratch), "%d", (int)_staged.txPowerDbm);
  keypadApplet().configureNumeric(_scratch, sizeof(_scratch) - 1, "TX power (dBm)",
                                  &RadioSettingsPanel::onTxDone, this);
  if (_host) _host->push(&keypadApplet());
}

bool RadioSettingsPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev != InputEvent::Select) return false;   // Back bubbles -> commit happens in onHide
  switch (_list.selected()) {
    case 0:
      radioPresetPickerApplet().configure(this);
      if (_host) _host->push(&radioPresetPickerApplet());
      return true;
    case 1: editFrequency(); return true;
    case 2:
      radioValuePickerApplet().configure(this, RadioField::Bandwidth, "Bandwidth");
      if (_host) _host->push(&radioValuePickerApplet());
      return true;
    case 3:
      radioValuePickerApplet().configure(this, RadioField::SF, "Spreading factor");
      if (_host) _host->push(&radioValuePickerApplet());
      return true;
    case 4:
      radioValuePickerApplet().configure(this, RadioField::CR, "Coding rate");
      if (_host) _host->push(&radioValuePickerApplet());
      return true;
    case 5: editTxPower(); return true;
    default:
      repeaterApplet().configure(this);
      if (_host) _host->push(&repeaterApplet());
      return true;
  }
}

RadioSettingsPanel& radioSettings() {
  static RadioSettingsPanel s;
  return s;
}

}  // namespace mishmesh
