// mishmesh/applets/RepeaterRadioPanel.cpp
#include <mishmesh/applets/RepeaterRadioPanel.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/RadioValuePickerApplet.h>
#include <mishmesh/applets/RadioPresetPickerApplet.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/applets/settings/RadioFormat.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/core/CliRequest.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace mishmesh {

static const int N = 6;    // field count; Save button is at focus N in the FormView

void RepeaterRadioPanel::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterRadioPanel::stageFrequency(float mhz) {
  if (mhz < 150.0f) mhz = 150.0f; else if (mhz > 2500.0f) mhz = 2500.0f;
  _cfg.freqMhz = mhz; _radioDirty = true;
}
void RepeaterRadioPanel::stageBandwidth(float khz) { _cfg.bwKhz = khz; _radioDirty = true; }
void RepeaterRadioPanel::stageSf(uint8_t sf) { _cfg.sf = sf; _radioDirty = true; }
void RepeaterRadioPanel::stageCr(uint8_t cr) { _cfg.cr = cr; _radioDirty = true; }
void RepeaterRadioPanel::stageTxPower(int dbm) {
  if (dbm < -9) dbm = -9; else if (dbm > 30) dbm = 30;
  _cfg.txPowerDbm = (int8_t)dbm; _txDirty = true;
}
void RepeaterRadioPanel::stagePreset(int i) {
  if (i < 0 || i >= PRESET_COUNT) return;
  _cfg.freqMhz = PRESETS[i].freq; _cfg.bwKhz = PRESETS[i].bw;
  _cfg.sf = PRESETS[i].sf; _cfg.cr = PRESETS[i].cr; _radioDirty = true;
}

void RepeaterRadioPanel::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.contacts; _app = ctx.app;
  _view.reset();
  _radioDirty = _txDirty = _rebootNote = _saveFailed = false;
  _phase = Phase::Loading; _step = Step::Radio;
  fireGet("get radio", _host ? _host->nowMs() : 0);
  if (_host) _host->requestRender();
}

void RepeaterRadioPanel::onForeground() {
  if (_phase == Phase::Editing) _phase = Phase::List;   // returned from picker/keypad
}

void RepeaterRadioPanel::fireGet(const char* cmd, uint32_t nowMs) {
  _startSeq = cliFire(_svc, _req, _pub, cmd, nowMs, 20000);
}
void RepeaterRadioPanel::fireSet(const char* cmd, uint32_t nowMs) { fireGet(cmd, nowMs); }

void RepeaterRadioPanel::beginSave() {
  _saveFailed = false; _rebootNote = false;
  if (_radioDirty) {
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "set radio %g %g %u %u", _cfg.freqMhz, _cfg.bwKhz,
             (unsigned)_cfg.sf, (unsigned)_cfg.cr);
    _step = Step::Radio; _phase = Phase::Saving; fireSet(cmd, _host ? _host->nowMs() : 0);
  } else if (_txDirty) {
    char cmd[24];
    snprintf(cmd, sizeof(cmd), "set tx %d", (int)_cfg.txPowerDbm);
    _step = Step::Tx; _phase = Phase::Saving; fireSet(cmd, _host ? _host->nowMs() : 0);
  }
  if (_host) _host->requestRender();
}

void RepeaterRadioPanel::onEditDone(void* ctx, const char* text) {
  auto* self = static_cast<RepeaterRadioPanel*>(ctx);
  if (!text) return;
  if (self->_editField == 1) self->stageFrequency((float)atof(text));
  else if (self->_editField == 5) self->stageTxPower(atoi(text));
}

int RepeaterRadioPanel::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  const Font* cap = fontCaption();
  const Font* body = fontBody();
  int caph = c.lineHeight(cap);

  // drive fetch/save
  if ((_phase == Phase::Loading || _phase == Phase::Saving) && _svc) {
    bool ok = false; const char* resp = nullptr;
    CliPoll st = cliPoll(_svc, _req, _pub, _startSeq, c.now(), ok, resp);
    if (st == CliPoll::Ready) {
      {
        const char* v = resp ? resp : "";
        if (_phase == Phase::Loading) {
          if (v[0] == '>' && v[1] == ' ') v += 2;
          if (_step == Step::Radio) {
            // parse "f,bw,sf,cr"
            char buf[40]; copyStr(buf, sizeof(buf), v);
            char* s = buf; char* f = strsep(&s, ","); char* b = strsep(&s, ",");
            char* sf = strsep(&s, ","); char* cr = strsep(&s, ",");
            if (f) _cfg.freqMhz = (float)atof(f);
            if (b) _cfg.bwKhz = (float)atof(b);
            if (sf) _cfg.sf = (uint8_t)atoi(sf);
            if (cr) _cfg.cr = (uint8_t)atoi(cr);
            _step = Step::Tx; fireGet("get tx", c.now());
          } else {   // Tx
            _cfg.txPowerDbm = (int8_t)atoi(v);
            _phase = Phase::List;
          }
        } else {   // Saving
          bool okReply = (v[0] == 'O' && v[1] == 'K');
          if (!okReply) { _saveFailed = true; _phase = Phase::List; }
          else if (_step == Step::Radio) {
            _radioDirty = false; _rebootNote = true;
            if (_txDirty) {
              char cmd[24]; snprintf(cmd, sizeof(cmd), "set tx %d", (int)_cfg.txPowerDbm);
              _step = Step::Tx; fireSet(cmd, c.now());
            } else _phase = Phase::List;
          } else { _txDirty = false; _phase = Phase::List; }
        }
      }
    } else if (st == CliPoll::TimedOut) {
      if (_phase == Phase::Loading) {
        if (_step == Step::Radio) { _step = Step::Tx; fireGet("get tx", c.now()); }
        else _phase = Phase::List;
      } else { _saveFailed = true; _phase = Phase::List; }
    }
  }

  int bh = drawTopBar(c, _bar, "Radio", _app, w);
  int top = bh + 1, bottom = h - caph - 1;

  // Loading screen: while fetching, show a centered "Loading..." and skip the form.
  if (_phase == Phase::Loading) {
    c.drawTextCentered(body, 0, top, w, bottom - top, "Loading...", DisplayDriver::LIGHT);
  } else {
    // Build FormRow array from current staged config then draw via FormView.
    char vbuf[5][24];
    FormRow rows[N];
    const RadioConfig& cfg = _cfg;
    int m = matchPreset(cfg.freqMhz, cfg.bwKhz, cfg.sf, cfg.cr);
    rows[0] = { "Preset",        m >= 0 ? PRESETS[m].name : "Custom" };
    rows[1] = { "Frequency",     formatRadioField(vbuf[0], sizeof(vbuf[0]), 1, cfg) };
    rows[2] = { "Bandwidth",     formatRadioField(vbuf[1], sizeof(vbuf[1]), 2, cfg) };
    rows[3] = { "Spread factor", formatRadioField(vbuf[2], sizeof(vbuf[2]), 3, cfg) };
    rows[4] = { "Coding rate",   formatRadioField(vbuf[3], sizeof(vbuf[3]), 4, cfg) };
    rows[5] = { "TX power",      formatRadioField(vbuf[4], sizeof(vbuf[4]), 5, cfg) };
    _view.draw(c, 0, top, w, bottom - top, rows, N, "Save");
  }

  const char* status = "";
  if (_phase == Phase::Saving) status = "Saving...";
  else if (_saveFailed) status = "Save failed";
  else if (_rebootNote) status = "Saved - reboot to apply";
  else if (anyDirty()) status = "Select Save to apply";
  c.drawText(cap, 2, bottom, status, DisplayDriver::LIGHT, TextAlign::Left);

  if (_phase == Phase::Confirm) _confirm.draw(c, 0, 0, w, h);

  if (_phase == Phase::Loading || _phase == Phase::Saving) return 150;
  return 500;
}

bool RepeaterRadioPanel::onInput(InputEvent ev) {
  if (_phase == Phase::Confirm) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r == ConfirmResult::Confirmed) { if (_host) _host->pop(); }
      else if (r == ConfirmResult::Cancelled) _phase = Phase::List;
    }
    return true;
  }

  // FormView owns NavUp/Down focus movement; Save button is at focus N.
  if (_view.onInput(ev, N, true)) { if (_host) _host->requestRender(); return true; }

  if (ev == InputEvent::Select) {
    int f = _view.focus();
    if (f == N) { beginSave(); return true; }   // Save button
    switch (f) {
      case 0: radioPresetPickerApplet().configure(this); _phase = Phase::Editing;
              if (_host) _host->push(&radioPresetPickerApplet()); return true;
      case 1: _editField = 1; _scratch[0] = 0;
              keypadApplet().configureNumeric(_scratch, sizeof(_scratch) - 1, "Frequency (MHz)", &RepeaterRadioPanel::onEditDone, this);
              _phase = Phase::Editing; if (_host) _host->push(&keypadApplet()); return true;
      case 2: radioValuePickerApplet().configure(this, RadioField::Bandwidth, "Bandwidth"); _phase = Phase::Editing;
              if (_host) _host->push(&radioValuePickerApplet()); return true;
      case 3: radioValuePickerApplet().configure(this, RadioField::SF, "Spreading factor"); _phase = Phase::Editing;
              if (_host) _host->push(&radioValuePickerApplet()); return true;
      case 4: radioValuePickerApplet().configure(this, RadioField::CR, "Coding rate"); _phase = Phase::Editing;
              if (_host) _host->push(&radioValuePickerApplet()); return true;
      case 5: _editField = 5; _scratch[0] = 0;
              keypadApplet().configureNumeric(_scratch, sizeof(_scratch) - 1, "TX power (dBm)", &RepeaterRadioPanel::onEditDone, this);
              _phase = Phase::Editing; if (_host) _host->push(&keypadApplet()); return true;
    }
    return true;
  }

  if (ev == InputEvent::Back && anyDirty()) {
    _confirm.configure("Discard changes?"); _phase = Phase::Confirm; return true;
  }
  return false;   // clean Back bubbles
}

RepeaterRadioPanel& repeaterRadioPanel() {
  static RepeaterRadioPanel s_radio;
  return s_radio;
}

}  // namespace mishmesh
