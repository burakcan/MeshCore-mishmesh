#include <mishmesh/applets/SetPathApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>

namespace mishmesh {

void SetPathApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _editingHash = false;
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
}

const char* SetPathApplet::label(int i) const {
  if (i == 0) return "Hash size";
  if (i == 1) return "Path";
  return "Save";
}
const char* SetPathApplet::value(int i) const {
  if (i == 0) {
    if (_hsLabel && _hash) { _hsLabel(*_hash, _val, sizeof(_val)); return _val; }
    return "";
  }
  if (i == 1) {
    if (_pathFmt && _path) { _pathFmt(_path, _val, sizeof(_val)); return _val; }
    return _path ? _path : "";
  }
  return nullptr;
}

int SetPathApplet::onRender(Canvas& c) {
  int w = c.width();
  int bh = drawTopBar(c, _bar, _title, _app, w);
  _list.draw(c, 0, bh + 1, w, c.height() - bh - 1);
  if (_editingHash) { _stepper.draw(c, 0, 0, c.width(), c.height()); return 100; }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 750;
}

bool SetPathApplet::onInput(InputEvent ev) {
  if (_editingHash) {
    // NavUp/Down map to Right/Left so the stepper increments/decrements naturally.
    InputEvent fwd = ev;
    if (ev == InputEvent::NavUp)   fwd = InputEvent::NavRight;
    if (ev == InputEvent::NavDown) fwd = InputEvent::NavLeft;
    if (_stepper.onInput(fwd)) {
      StepperResult r = _stepper.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _hash) *_hash = _stepper.value();
        _editingHash = false;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while the stepper modal is open
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int sel = _list.selected();
    if (sel == 0) {
      _stepper.configure("Hash size", _hash ? *_hash : _hsMin, _hsMin, _hsMax, _hsLabel);
      _editingHash = true;
    } else if (sel == 1) {
      keypadApplet().configure(_path, _pathCap - 1, "Path (hex)", nullptr, nullptr);
      if (_host) _host->push(&keypadApplet());
    } else {   // Save
      if (_submit && _submit(_ctx)) { if (_host) _host->pop(); }
    }
    return true;
  }
  return false;   // Back bubbles -> host pops
}

SetPathApplet& setPathApplet() {
  static SetPathApplet s;
  return s;
}

}  // namespace mishmesh
