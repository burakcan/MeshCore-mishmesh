#include <mishmesh/applets/FormApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

// Compact layout: one row per field + centred button under a StatusBar.
// Focus/scroll/geometry are managed by FormView; FormApplet drives field
// semantics (keypad push, stepper modal, validation, submit callback).

void FormApplet::configure(const char* title, const Field* fields, int n,
                           FormSubmitFn onSubmit, void* ctx,
                           const char* submitLabel) {
  _title = title ? title : "";
  _submitLabel = submitLabel ? submitLabel : "Submit";
  if (n > MAX_FIELDS) n = MAX_FIELDS;
  _n = n;
  for (int i = 0; i < n; i++) _fields[i] = fields[i];
  _onSubmit = onSubmit;
  _ctx = ctx;
}

void FormApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app = ctx.app;
  _view.reset();
  _editingStepper = false;
  _stepperField = -1;
}

int FormApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();

  // Top bar (title + battery), mirroring HomeApplet.
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle(_title);
  if (_app) _bar.setBattery(_app->batteryMillivolts());
  _bar.draw(c, 0, 0, w, barH);

  const int bodyY = barH + 2;
  const int bodyH = h - bodyY;

  // Build pre-formatted rows for FormView.
  char tmps[MAX_FIELDS][24];
  FormRow rows[MAX_FIELDS];
  for (int i = 0; i < _n; i++) {
    rows[i].label = _fields[i].label;
    const char* val = _fields[i].buf;
    if (_fields[i].kind == Stepper) {
      if (_fields[i].ifmt && _fields[i].ival) _fields[i].ifmt(*_fields[i].ival, tmps[i], sizeof(tmps[i]));
      else tmps[i][0] = 0;
      val = tmps[i];
    } else if (_fields[i].display) {
      _fields[i].display(_fields[i].buf ? _fields[i].buf : "", tmps[i], sizeof(tmps[i]));
      val = tmps[i];
    }
    rows[i].value = val;
  }
  _view.draw(c, 0, bodyY, w, bodyH, rows, _n, _submitLabel);

  if (_editingStepper) { _stepper.draw(c, 0, 0, w, h); return 100; }
  return 500;
}

bool FormApplet::submit() {
  for (int i = 0; i < _n; i++) {
    if (_fields[i].kind == Stepper) continue;   // clamped by the modal; always valid
    const char* s = _fields[i].buf;
    bool ok = _fields[i].validate ? _fields[i].validate(s) : (s && s[0] != 0);
    if (!ok) {
      _view.setFocus(i);
      if (_host) _host->postToast(_fields[i].errMsg ? _fields[i].errMsg : "Required");
      return false;
    }
  }
  return _onSubmit ? _onSubmit(_ctx) : true;
}

bool FormApplet::onInput(InputEvent ev) {
  if (_editingStepper) {
    if (_stepper.onInput(ev)) {
      StepperResult r = _stepper.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _stepperField >= 0 && _fields[_stepperField].ival)
          *_fields[_stepperField].ival = _stepper.value();
        _editingStepper = false;
        _stepperField = -1;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_view.onInput(ev, _n, true)) return true;

  if (ev == InputEvent::Select) {
    const int f = _view.focus();
    if (f < _n && _fields[f].kind == Stepper) {
      const Field& fd = _fields[f];
      _stepper.configure(fd.label, fd.ival ? *fd.ival : fd.imin, fd.imin, fd.imax, fd.ifmt);
      _editingStepper = true;
      _stepperField = f;
    } else if (f < _n) {
      keypadApplet().configure(_fields[f].buf, _fields[f].cap,
                               _fields[f].label, &FormApplet::noopConfirm, nullptr);
      if (_host) _host->push(&keypadApplet());
    } else if (submit() && _host) {
      _host->pop();
    }
    return true;
  }
  return false;   // Back/Cancel bubble -> host pops (cancel)
}

FormApplet& formApplet() {
  static FormApplet a;
  return a;
}

}  // namespace mishmesh
