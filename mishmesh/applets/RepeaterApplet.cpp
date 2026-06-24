#include <mishmesh/applets/RepeaterApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <math.h>
#include <stdio.h>

namespace mishmesh {

static const char* INFO =
  "Repeats messages for nearby nodes on a shared off-grid frequency. "
  "Leaves your normal mesh while active.";

RepeaterApplet::RepeaterApplet() : Applet("Repeater") {}

int RepeaterApplet::count() const {
  return 1 + (_app ? _app->repeatFreqCount() : 0);   // Off + allowed frequencies
}

const char* RepeaterApplet::label(int i) const {
  if (i == 0) return "Off";
  snprintf(_lbl, sizeof(_lbl), "%.3f MHz", _app ? _app->repeatFreqMhz(i - 1) : 0.0f);
  return _lbl;
}

bool RepeaterApplet::radioOn(int i) const {
  if (!_tgt) return i == 0;
  const RadioConfig& c = _tgt->stagedConfig();
  if (i == 0) return !c.repeater;
  if (!c.repeater || !_app) return false;
  return fabsf(_app->repeatFreqMhz(i - 1) - c.freqMhz) < 0.001f;
}

void RepeaterApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
  for (int i = 0; i < count(); i++) if (radioOn(i)) { _list.setSelected(i); break; }
}

int RepeaterApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle("Repeater");
  _bar.draw(c, 0, 0, w, barH);
  int y = c.drawTextWrapped(fontBody(), 2, barH + 2, w - 4, INFO, DisplayDriver::LIGHT);
  y += 2;
  _list.draw(c, 0, y, w, h - y);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool RepeaterApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _tgt) {
    int i = _list.selected();
    const RadioConfig& c = _tgt->stagedConfig();
    if (i == 0) {                                   // Off -> restore + disable
      if (!c.repeater) return true;                 // already off: no-op (avoid a needless apply)
      if (_app) { float s = _app->savedRepeatFreq(); if (s > 0.0f) _tgt->stageFrequency(s); }
      _tgt->stageRepeater(false);
    } else {                                        // enable on a chosen frequency
      float f = _app ? _app->repeatFreqMhz(i - 1) : 0.0f;
      if (c.repeater && fabsf(c.freqMhz - f) < 0.001f) return true;   // already on this freq: no-op
      if (!c.repeater && _app) _app->setSavedRepeatFreq(c.freqMhz);   // remember pre-repeat freq
      if (_app) _tgt->stageFrequency(f);
      _tgt->stageRepeater(true);
    }
    return true;   // radio mark refreshes from stagedConfig() next render
  }
  return false;    // Back bubbles -> host pops
}

RepeaterApplet& repeaterApplet() {
  static RepeaterApplet s;
  return s;
}

}  // namespace mishmesh
