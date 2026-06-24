#include <mishmesh/applets/RepeaterApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <math.h>
#include <stdio.h>

namespace mishmesh {

// Kept to two lines: it scrolls with the list as a header, so on a short (160x80)
// screen the options must still be visible below it at the top of the scroll.
static const char* INFO =
  "Off-grid repeater. Leaves your normal mesh while on.";

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

void RepeaterApplet::InfoHeader::draw(Canvas& c, int x, int y, int w, int h) {
  (void)h;
  c.drawTextWrapped(fontBody(), x + 2, y, w - 4, text, DisplayDriver::LIGHT);
}

int RepeaterApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle("Repeater");
  _bar.setBattery(_app ? _app->batteryMillivolts() : 0);
  _bar.draw(c, 0, 0, w, barH);

  // Description + options scroll as one unit: the description is a ListMenu header,
  // so the whole body moves together as the selection walks the list.
  _info.text = INFO;
  _lastCaptionH = c.measureTextWrapped(fontBody(), w - 4, INFO);
  _list.setHeader(&_info, _lastCaptionH);
  int y = barH + 1;
  _lastBodyH = h - y;
  _list.draw(c, 0, y, w, _lastBodyH);
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
