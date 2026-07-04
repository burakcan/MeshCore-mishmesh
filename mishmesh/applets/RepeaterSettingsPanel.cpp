// mishmesh/applets/RepeaterSettingsPanel.cpp
#include <mishmesh/applets/RepeaterSettingsPanel.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/Modal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace mishmesh {

// The single shared staged store, only one panel is open at a time.
static char s_staged[RepeaterSettingsPanel::MAX_FIELDS][RepeaterSettingsPanel::FIELD_CAP];
static bool s_dirty[RepeaterSettingsPanel::MAX_FIELDS];
static bool s_fetched[RepeaterSettingsPanel::MAX_FIELDS];

const char* RepeaterSettingsPanel::displayValueForTest(int i) const {
  if (i < 0 || i >= _n) return "";
  if (_defs[i].kind == SettingFieldDef::Toggle)
    return strcmp(_engine.value(i), "on") == 0 ? "On" : "Off";
  const char* v = _engine.value(i);
  if (_defs[i].longValue) {
    strncpy(_truncBuf, v, 6); _truncBuf[6] = 0; strcat(_truncBuf, "...");
    return _truncBuf;
  }
  return v;
}

bool RepeaterSettingsPanel::hasEditable() const {
  for (int i = 0; i < _n; i++) {
    if (_defs[i].setCmd && _defs[i].kind != SettingFieldDef::ReadOnly) return true;
  }
  return false;
}

void RepeaterSettingsPanel::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterSettingsPanel::setModel(const SettingFieldDef* defs, int n, const char* title) {
  _defs = defs;
  _n = n < MAX_FIELDS ? n : MAX_FIELDS;
  _title = title ? title : "Settings";
}

void RepeaterSettingsPanel::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _app = ctx.app;
  _phase = Phase::List;
  _textBuilt = false;
  _view.reset();
  _engine.configure(_svc, _pub, _defs, _n, &s_staged[0][0], FIELD_CAP, s_dirty, s_fetched);
  _engine.beginFetch(_host ? _host->nowMs() : 0);
  if (_host) _host->requestRender();
}

void RepeaterSettingsPanel::onForeground() {
  // Returned from the keypad (OK or Cancel): back to the list so onRender resumes polling.
  if (_phase == Phase::Editing) _phase = Phase::List;
}

void RepeaterSettingsPanel::editField(int i) {
  const SettingFieldDef& f = _defs[i];
  if (f.kind == SettingFieldDef::ReadOnly) return;
  if (f.kind == SettingFieldDef::Toggle) {
    char* v = _engine.value(i);
    bool on = strcmp(v, "on") == 0;
    strcpy(v, on ? "off" : "on");
    s_dirty[i] = true;
    if (_host) _host->requestRender();
    return;
  }
  _editIdx = i;
  _phase = Phase::Editing;
  char title[40];
  if (f.kind == SettingFieldDef::Number && (f.minVal != f.maxVal))
    snprintf(title, sizeof(title), "%s (%ld-%ld)", f.label, (long)f.minVal, (long)f.maxVal);
  else { copyStr(title, sizeof(title), f.label); }
  char* buf = _engine.value(i);
  if (f.kind == SettingFieldDef::Number || f.kind == SettingFieldDef::Float)
    keypadApplet().configureNumeric(buf, FIELD_CAP - 1, title, &RepeaterSettingsPanel::onEditDone, this);
  else
    keypadApplet().configure(buf, FIELD_CAP - 1, title, &RepeaterSettingsPanel::onEditDone, this);
  if (_host) _host->push(&keypadApplet());
}

void RepeaterSettingsPanel::openModal(int i) {
  _modalText.clear();
  const char* v = _engine.value(i);
  int len = (int)strlen(v);
  // Split full value into lines of up to 16 chars for the ScrollText modal.
  for (int off = 0; off < len; off += 16) {
    char line[17];
    int chars = len - off;
    if (chars > 16) chars = 16;
    strncpy(line, v + off, chars);
    line[chars] = 0;
    _modalText.addLine(line);
  }
  _phase = Phase::Modal;
  if (_host) _host->requestRender();
}

void RepeaterSettingsPanel::onEditDone(void* ctx, const char* text) {
  auto* self = static_cast<RepeaterSettingsPanel*>(ctx);
  int i = self->_editIdx;
  if (i < 0 || i >= self->_n) return;
  char* buf = self->_engine.value(i);
  if (text && text != buf) { strncpy(buf, text, RepeaterSettingsPanel::FIELD_CAP - 1); buf[RepeaterSettingsPanel::FIELD_CAP - 1] = 0; }
  // Integer clamp for Number (not Float; atol would truncate "37.7").
  const SettingFieldDef& f = self->_defs[i];
  if (f.kind == SettingFieldDef::Number && f.minVal != f.maxVal) {
    long v = atol(buf);
    if (v < f.minVal) v = f.minVal; else if (v > f.maxVal) v = f.maxVal;
    snprintf(buf, RepeaterSettingsPanel::FIELD_CAP, "%ld", v);
  }
  s_dirty[i] = true;
}

int RepeaterSettingsPanel::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  const Font* cap = fontCaption();
  const Font* body = fontBody();
  int caph = c.lineHeight(cap);

  if (_phase != Phase::Editing) _engine.poll(_svc ? _svc->cliSeq() : 0, c.now());

  int bh = drawTopBar(c, _bar, _title, _app, w);
  int top = bh + 1;
  int bottom = h - caph - 1;

  RemoteSettingsEngine::Phase ep = _engine.phase();

  // Loading screen: while fetching, show a centered "Loading..." and skip the form.
  if (ep == RemoteSettingsEngine::Phase::Fetching) {
    c.drawTextCentered(body, 0, top, w, bottom - top, "Loading...", DisplayDriver::LIGHT);
    return 150;
  }

  // Render area below the status bar.
  int bodyY = top;
  int bodyH = bottom - top;
  if (!hasEditable()) {
    // Read-only text mode (e.g. Version panel): show fetched values as plain wrapped text.
    if (!_textBuilt) {
      _textView.clear();
      _textView.setWrap(true);   // long read-only values (e.g. the version string) wrap, not ellipsize
      for (int i = 0; i < _n; i++) {
        const char* v = _engine.value(i);
        if (v && *v) _textView.addLine(v);
      }
      _textBuilt = true;
    }
    _textView.draw(c, 0, bodyY, w, bodyH);
  } else {
    // Form mode: boxed label+value rows + "Save" button at focus _n.
    FormRow rows[MAX_FIELDS];
    for (int i = 0; i < _n; i++) {
      rows[i].label = _defs[i].label;
      if (_defs[i].kind == SettingFieldDef::Toggle) {
        rows[i].value = strcmp(_engine.value(i), "on") == 0 ? "On" : "Off";
      } else if (_defs[i].longValue) {
        strncpy(_truncBuf, _engine.value(i), 6); _truncBuf[6] = 0; strcat(_truncBuf, "...");
        rows[i].value = _truncBuf;
      } else {
        rows[i].value = _engine.value(i);
      }
    }
    _view.draw(c, 0, bodyY, w, bodyH, rows, _n, "Save");
  }

  // Status line at the very bottom.
  const char* status = "";
  char sbuf[40];
  if (ep == RemoteSettingsEngine::Phase::Saving) {
    snprintf(sbuf, sizeof(sbuf), "Saving %d/%d...", _engine.activeIndex() + 1, _n); status = sbuf;
  } else if (ep == RemoteSettingsEngine::Phase::Error) {
    int ei = _engine.errorIndex();
    snprintf(sbuf, sizeof(sbuf), "Save failed: %s", (ei >= 0 && ei < _n) ? _defs[ei].label : "?"); status = sbuf;
  } else if (hasEditable() && _engine.anyDirty()) {
    status = "Select Save to apply";
  }
  c.drawText(cap, 2, bottom, status, DisplayDriver::LIGHT, TextAlign::Left);

  if (_phase == Phase::Confirm) _confirm.draw(c, 0, 0, w, h);

  if (_phase == Phase::Modal) {
    Canvas box = drawModalChrome(c);
    _modalText.draw(box, 2, 2, box.width() - 4, box.height() - 4);
  }

  if (ep == RemoteSettingsEngine::Phase::Saving) return 150;
  if (!hasEditable()) return _textView.needsAnimation() ? 33 : 1000;
  return 1000;   // idle: nav/edit paths call requestRender(), so no faster tick needed
}

bool RepeaterSettingsPanel::onInput(InputEvent ev) {
  if (_phase == Phase::Confirm) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r == ConfirmResult::Confirmed) { if (_host) _host->pop(); }
      else if (r == ConfirmResult::Cancelled) { _phase = Phase::List; }
    }
    return true;
  }

  if (_phase == Phase::Modal) {
    if (ev == InputEvent::Back || ev == InputEvent::Select) {
      _phase = Phase::List;
      if (_host) _host->requestRender();
      return true;
    }
    _modalText.onInput(ev);
    if (_host) _host->requestRender();
    return true;
  }

  // Read-only text mode: NavUp/Down scroll the text view; other inputs bubble.
  if (!hasEditable()) {
    if (_textView.onInput(ev)) { if (_host) _host->requestRender(); return true; }
    return false;
  }

  // Form mode: FormView owns NavUp/Down focus movement.
  if (_view.onInput(ev, _n, true)) { if (_host) _host->requestRender(); return true; }

  if (ev == InputEvent::Select) {
    int f = _view.focus();
    if (f == _n) {
      // Save button.
      _engine.beginSave(_host ? _host->nowMs() : 0);
      if (_host) _host->requestRender();
      return true;
    }
    // Field row.
    const SettingFieldDef& def = _defs[f];
    if (def.longValue && def.kind == SettingFieldDef::ReadOnly) {
      openModal(f);
      return true;
    }
    editField(f);
    return true;
  }

  if (ev == InputEvent::Back && _engine.anyDirty()) {
    _confirm.configure("Discard changes?");
    _phase = Phase::Confirm;
    return true;   // swallow Back; the confirm decides
  }
  return false;    // clean Back bubbles -> host pops
}

RepeaterSettingsPanel& repeaterSettingsPanel() {
  static RepeaterSettingsPanel s_panel;
  return s_panel;
}

}  // namespace mishmesh
