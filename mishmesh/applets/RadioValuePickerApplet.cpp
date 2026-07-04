#include <mishmesh/applets/RadioValuePickerApplet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

// Standard LoRa bandwidth set (kHz). SF/CR are contiguous ranges.
static const float BW_OPTS[] = {7.8f, 10.4f, 15.6f, 20.8f, 31.25f, 41.7f, 62.5f, 125.0f, 250.0f, 500.0f};
static const float SF_OPTS[] = {5, 6, 7, 8, 9, 10, 11, 12};
static const float CR_OPTS[] = {5, 6, 7, 8};
static const int BW_N = (int)(sizeof(BW_OPTS) / sizeof(BW_OPTS[0]));
static const int SF_N = (int)(sizeof(SF_OPTS) / sizeof(SF_OPTS[0]));
static const int CR_N = (int)(sizeof(CR_OPTS) / sizeof(CR_OPTS[0]));

RadioValuePickerApplet::RadioValuePickerApplet() : Applet("Radio") {}

void RadioValuePickerApplet::configure(RadioStagingTarget* tgt, RadioField field, const char* title) {
  _tgt = tgt; _field = field;
  copyStr(_title, sizeof(_title), title ? title : "Value");
}

const float* RadioValuePickerApplet::options() const {
  if (_field == RadioField::Bandwidth) return BW_OPTS;
  if (_field == RadioField::CR) return CR_OPTS;
  return SF_OPTS;
}

int RadioValuePickerApplet::count() const {
  if (_field == RadioField::Bandwidth) return BW_N;
  if (_field == RadioField::CR) return CR_N;
  return SF_N;
}

int RadioValuePickerApplet::currentIndex() const {
  if (!_tgt) return 0;
  const RadioConfig& c = _tgt->stagedConfig();
  const float* o = options();
  int n = count();
  for (int i = 0; i < n; i++) {
    float target = (_field == RadioField::Bandwidth) ? c.bwKhz
                 : (_field == RadioField::CR) ? (float)c.cr : (float)c.sf;
    if (fabsf(o[i] - target) < 0.05f) return i;
  }
  return 0;
}

const char* RadioValuePickerApplet::label(int i) const {
  const float* o = options();
  if (_field == RadioField::Bandwidth) snprintf(_lbl, sizeof(_lbl), "%g kHz", o[i]);
  else if (_field == RadioField::CR)   snprintf(_lbl, sizeof(_lbl), "CR%d", (int)o[i]);
  else                                 snprintf(_lbl, sizeof(_lbl), "SF%d", (int)o[i]);
  return _lbl;
}

bool RadioValuePickerApplet::radioOn(int i) const { return currentIndex() == i; }

void RadioValuePickerApplet::onStart(AppletContext& ctx) {
  (void)ctx;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
  _list.setSelected(currentIndex());
}

int RadioValuePickerApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle(_title);
  _bar.draw(c, 0, 0, w, barH);
  _list.draw(c, 0, barH, w, h - barH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool RadioValuePickerApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _tgt) {
    float v = options()[_list.selected()];
    switch (_field) {
      case RadioField::Bandwidth: _tgt->stageBandwidth(v); break;
      case RadioField::CR:        _tgt->stageCr((uint8_t)v); break;
      default:                    _tgt->stageSf((uint8_t)v); break;
    }
    return true;   // radio mark refreshes from stagedConfig() next render
  }
  return false;    // Back bubbles -> host pops
}

RadioValuePickerApplet& radioValuePickerApplet() {
  static RadioValuePickerApplet s;
  return s;
}

}  // namespace mishmesh
