#include <mishmesh/applets/RadioPresetPickerApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/settings/RadioPresets.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

RadioPresetPickerApplet::RadioPresetPickerApplet() : Applet("Preset") {}

int RadioPresetPickerApplet::count() const { return PRESET_COUNT; }

const char* RadioPresetPickerApplet::label(int i) const {
  if (i < 0 || i >= PRESET_COUNT) return "";
  return PRESETS[i].name;
}

const char* RadioPresetPickerApplet::value(int i) const {
  if (i < 0 || i >= PRESET_COUNT) return "";
  const RadioPreset& p = PRESETS[i];
  snprintf(_summary, sizeof(_summary), "%gMHz/SF%u/BW%g/CR%u",
           p.freq, (unsigned)p.sf, p.bw, (unsigned)p.cr);
  return _summary;
}

bool RadioPresetPickerApplet::radioOn(int i) const {
  if (!_tgt) return false;
  const RadioConfig& c = _tgt->stagedConfig();
  return matchPreset(c.freqMhz, c.bwKhz, c.sf, c.cr) == i;
}

void RadioPresetPickerApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _list.setRowHeight(16);   // taller rows: name + summary
  _list.setModel(this);
  _list.resetSelection();
  if (_tgt) {
    const RadioConfig& c = _tgt->stagedConfig();
    int m = matchPreset(c.freqMhz, c.bwKhz, c.sf, c.cr);
    if (m >= 0) _list.setSelected(m);
  }
}

int RadioPresetPickerApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int barH = drawTopBar(c, _bar, "Presets", _app, w);   // title + battery
  _list.draw(c, 0, barH + 1, w, h - barH - 1);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool RadioPresetPickerApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _tgt) {
    _tgt->stagePreset(_list.selected());
    return true;
  }
  return false;   // Back bubbles -> host pops
}

RadioPresetPickerApplet& radioPresetPickerApplet() {
  static RadioPresetPickerApplet s;
  return s;
}

}  // namespace mishmesh
