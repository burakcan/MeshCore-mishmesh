#include <mishmesh/applets/FormApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

// Compact layout (px): one row per field — label on the left, a boxed value on
// the right — then a centred button, under a StatusBar top bar. Focus inverts
// (fills) the focused box/button. Tall forms scroll to keep the focused item
// visible. A Stepper field's value is picked via a modal StepperDialog overlay.
static const int ROW_H   = 17;   // field row pitch
static const int BOX_H   = 14;   // input box height
static const int BTN_H   = 14;   // button height
static const int BTN_GAP = 5;    // gap above the button
static const int PAD     = 4;

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
  _focus = 0;
  _scroll = 0;
  _editingStepper = false;
  _stepperField = -1;
}

int FormApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  const mf_font_s* lf = fontCaption();   // field labels
  const mf_font_s* vf = fontBody();      // values / button

  // Top bar (title + battery), mirroring HomeApplet.
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle(_title);
  if (_app) _bar.setBattery(_app->batteryMillivolts());
  _bar.draw(c, 0, 0, w, barH);

  const int bodyY = barH + 2;
  const int bodyH = h - bodyY;

  // Label column = widest label, clamped to a third of the screen.
  int labelW = 0;
  for (int i = 0; i < _n; i++) {
    int lw = c.textWidth(lf, _fields[i].label);
    if (lw > labelW) labelW = lw;
  }
  const int maxLabel = w / 3;
  if (labelW > maxLabel) labelW = maxLabel;
  const int boxX = PAD + labelW + PAD;
  const int boxW = w - boxX - PAD;

  // Button geometry (content coords, before scroll).
  const int btnTop = _n * ROW_H + BTN_GAP;
  int btnW = c.textWidth(vf, _submitLabel) + 18;
  if (btnW > w - 2 * PAD) btnW = w - 2 * PAD;
  const int btnX = (w - btnW) / 2;

  // Scroll so the focused item is fully visible.
  const int contentH = btnTop + BTN_H;
  int focTop, focBot;
  if (_focus < _n) { focTop = _focus * ROW_H;     focBot = focTop + BOX_H; }
  else             { focTop = btnTop;             focBot = focTop + BTN_H; }
  if (focTop - _scroll < 0)          _scroll = focTop;
  else if (focBot - _scroll > bodyH) _scroll = focBot - bodyH;
  int maxScroll = contentH - bodyH;
  if (maxScroll < 0) maxScroll = 0;
  if (_scroll > maxScroll) _scroll = maxScroll;
  if (_scroll < 0) _scroll = 0;

  const int lh = c.fontHeight(lf);
  const int vh = c.fontHeight(vf);

  // Fields.
  for (int i = 0; i < _n; i++) {
    const int rowY = bodyY + i * ROW_H - _scroll;
    if (rowY + BOX_H < bodyY || rowY > bodyY + bodyH) continue;   // off-screen
    const bool foc = (_focus == i);
    c.drawText(lf, PAD, rowY + (BOX_H - lh) / 2, _fields[i].label, DisplayDriver::LIGHT);
    if (foc) c.fillRect(boxX, rowY, boxW, BOX_H, DisplayDriver::LIGHT);
    else     c.drawRect(boxX, rowY, boxW, BOX_H, DisplayDriver::LIGHT);

    char tmp[24];
    const char* val = _fields[i].buf;
    if (_fields[i].kind == Stepper) {
      if (_fields[i].ifmt && _fields[i].ival) _fields[i].ifmt(*_fields[i].ival, tmp, sizeof(tmp));
      else tmp[0] = 0;
      val = tmp;
    } else if (_fields[i].display) {
      _fields[i].display(_fields[i].buf ? _fields[i].buf : "", tmp, sizeof(tmp));
      val = tmp;
    }
    if (val && val[0]) {
      c.drawTextEllipsized(vf, boxX + 3, rowY + (BOX_H - vh) / 2, boxW - 6, val,
                           foc ? DisplayDriver::DARK : DisplayDriver::LIGHT);
    }
  }

  // Button (centred; inverted when focused).
  const int btnY = bodyY + btnTop - _scroll;
  const bool bfoc = (_focus == _n);
  if (bfoc) c.fillRect(btnX, btnY, btnW, BTN_H, DisplayDriver::LIGHT);
  else      c.drawRect(btnX, btnY, btnW, BTN_H, DisplayDriver::LIGHT);
  c.drawTextEllipsized(vf, btnX + btnW / 2, btnY + (BTN_H - vh) / 2, btnW - 6,
                       _submitLabel, bfoc ? DisplayDriver::DARK : DisplayDriver::LIGHT,
                       TextAlign::Center);

  if (_editingStepper) { _stepper.draw(c, 0, 0, w, h); return 100; }
  return 500;
}

bool FormApplet::submit() {
  for (int i = 0; i < _n; i++) {
    if (_fields[i].kind == Stepper) continue;   // clamped by the modal; always valid
    const char* s = _fields[i].buf;
    bool ok = _fields[i].validate ? _fields[i].validate(s) : (s && s[0] != 0);
    if (!ok) {
      _focus = i;
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

  switch (ev) {
    case InputEvent::NavUp:   if (_focus > 0)  _focus--; return true;
    case InputEvent::NavDown: if (_focus < _n) _focus++; return true;
    case InputEvent::Select:
      if (_focus < _n && _fields[_focus].kind == Stepper) {
        const Field& f = _fields[_focus];
        _stepper.configure(f.label, f.ival ? *f.ival : f.imin, f.imin, f.imax, f.ifmt);
        _editingStepper = true;
        _stepperField = _focus;
      } else if (_focus < _n) {
        keypadApplet().configure(_fields[_focus].buf, _fields[_focus].cap,
                                 _fields[_focus].label, &FormApplet::noopConfirm, nullptr);
        if (_host) _host->push(&keypadApplet());
      } else if (submit() && _host) {
        _host->pop();
      }
      return true;
    default:
      return false;   // Back/Cancel bubble -> host pops (cancel)
  }
}

FormApplet& formApplet() {
  static FormApplet a;
  return a;
}

}  // namespace mishmesh
