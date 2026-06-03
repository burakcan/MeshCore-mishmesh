#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/InputSource.h>
#include <mishmesh/text/Fonts.h>
#include <helpers/ui/DisplayDriver.h>
#include <string.h>

namespace mishmesh {

static const uint32_t TOAST_MS = 1400;

AppletHost::AppletHost(DisplayDriver* display, const AppletContext& ctx)
    : _display(display), _canvas(display), _ctx(ctx),
      _depth(0), _nsources(0),
      _next_render_at(0), _has_rendered(false), _dirty(true),
      _auto_off_ms(30000), _last_activity(0), _activity_init(false),
      _last_input_event(InputEvent::None), _last_input_ms(0), _input_seen(false),
      _toast_until(0), _toast_pending(false) {
  for (int i = 0; i < MAX_STACK; i++) _stack[i] = nullptr;
  for (int i = 0; i < MAX_SOURCES; i++) _sources[i] = nullptr;
  _toast_msg[0] = 0;
  _ctx.host = this;
}

void AppletHost::postToast(const char* msg) {
  strncpy(_toast_msg, msg ? msg : "", sizeof(_toast_msg) - 1);
  _toast_msg[sizeof(_toast_msg) - 1] = 0;
  _toast_pending = true;   // stamped with a deadline on the next loop (needs now)
  _dirty = true;
}

void AppletHost::addSource(InputSource* src) {
  if (src == nullptr || _nsources >= MAX_SOURCES) return;
  _sources[_nsources++] = src;
}

Applet* AppletHost::foreground() const {
  return _depth > 0 ? _stack[_depth - 1] : nullptr;
}

void AppletHost::applyInputContext() {
  Applet* fg = foreground();
  bool backRepeat = fg != nullptr && fg->wantsBackRepeat();
  for (int i = 0; i < _nsources; i++) {
    if (_sources[i] != nullptr) _sources[i]->setHoldRepeat(backRepeat);
  }
}

void AppletHost::setRoot(Applet* root) {
  if (root == nullptr || _depth != 0) return;
  if (_display != nullptr && !_display->isOn()) _display->turnOn();  // panels boot off
  _stack[0] = root;
  _depth = 1;
  root->onStart(_ctx);
  root->onForeground();
  applyInputContext();
  _dirty = true;
}

void AppletHost::push(Applet* a) {
  if (a == nullptr || _depth >= MAX_STACK) return;
  Applet* cur = foreground();
  if (cur != nullptr) cur->onBackground();
  _stack[_depth++] = a;
  a->onStart(_ctx);
  a->onForeground();
  applyInputContext();
  _dirty = true;
}

void AppletHost::pop() {
  if (_depth <= 1) return;   // keep the root
  Applet* top = _stack[_depth - 1];
  top->onBackground();
  top->onStop();
  _stack[--_depth] = nullptr;
  Applet* revealed = foreground();
  if (revealed != nullptr) revealed->onForeground();
  applyInputContext();
  _dirty = true;
}

void AppletHost::replace(Applet* a) {
  if (a == nullptr || _depth == 0) return;
  Applet* top = _stack[_depth - 1];
  top->onBackground();
  top->onStop();
  _stack[_depth - 1] = a;
  a->onStart(_ctx);
  a->onForeground();
  applyInputContext();
  _dirty = true;
}

void AppletHost::dispatch(InputEvent ev, bool repeat) {
  Applet* fg = foreground();
  if (fg == nullptr) return;
  bool consumed = fg->onInput(ev);
  // A long-press the screen doesn't use falls back to a normal press, so an
  // over-long center press still activates (e.g. opens the focused menu item)
  // instead of silently doing nothing. Screens that handle SelectLong consume it.
  if (!consumed && ev == InputEvent::SelectLong) consumed = fg->onInput(InputEvent::Select);
  // Auto-repeat (held button) must never pop: a hold that empties a text field
  // should stop there, not exit - and never cascade through screens. Only a
  // fresh, unconsumed Back pops.
  if (!consumed && ev == InputEvent::Back && !repeat) {
    pop();
  }
  _dirty = true;
}

void AppletHost::pumpInput(uint32_t now_ms) {
  for (int i = 0; i < _nsources; i++) {
    if (_sources[i] == nullptr) continue;
    InputReport rep;                       // fresh each poll: `repeat` defaults false
    while ((rep = InputReport(), _sources[i]->poll(rep))) {
      _last_activity = now_ms;
      if (_display != nullptr && !_display->isOn()) {
        _display->turnOn();   // first press only wakes; it isn't delivered
        _dirty = true;
      } else {
        bool bounce = _input_seen && rep.event == _last_input_event &&
                      now_ms - _last_input_ms < INPUT_DEBOUNCE_MS;
        _last_input_event = rep.event; _last_input_ms = now_ms; _input_seen = true;
        if (!bounce) dispatch(rep.event, rep.repeat);
      }
    }
  }
}

void AppletHost::loop(uint32_t now_ms) {
  if (!_activity_init) {
    _last_activity = now_ms;
    _activity_init = true;
  }

  pumpInput(now_ms);

  if (_toast_pending) {            // applet posted during input dispatch; stamp it now
    _toast_until = now_ms + TOAST_MS;
    _toast_pending = false;
    _dirty = true;
  }

  if (_auto_off_ms > 0 && _display != nullptr && _display->isOn() &&
      now_ms - _last_activity > _auto_off_ms) {
    _display->turnOff();
  }

  renderIfDue(now_ms);

  // Rendering is synchronous and a frame can take many ms to compose and flush to
  // the panel (a full-screen modal scrim is the worst case). Polling once more right
  // after the frame keeps that render time from becoming an input-blind gap wider
  // than the button debounce window, which would silently drop short taps. This is
  // framework-wide: any applet/modal, however heavy its draw, stays responsive
  // without needing per-screen tuning. Idle cost is nil - sources just report no
  // change. Events caught here paint on the next loop (_dirty), one frame later.
  pumpInput(now_ms);
}

void AppletHost::renderIfDue(uint32_t now_ms) {
  Applet* fg = foreground();
  if (fg == nullptr || _display == nullptr || !_display->isOn()) return;

  bool due = _dirty || !_has_rendered || now_ms >= _next_render_at;
  if (!due) return;

  _canvas.setNow(now_ms);
  _display->startFrame();
  int delay = fg->onRender(_canvas);
  if (now_ms < _toast_until) {
    int w = _canvas.width(), hh = 14, by = _canvas.height() - hh;
    _canvas.fillRect(0, by, w, hh, DisplayDriver::DARK);
    _canvas.drawRect(0, by, w, hh, DisplayDriver::LIGHT);
    _canvas.drawTextEllipsized(fontBody(), w / 2, by + (hh - _canvas.fontHeight(fontBody())) / 2,
                               w - 4, _toast_msg, DisplayDriver::LIGHT, TextAlign::Center);
    uint32_t rem = _toast_until - now_ms;          // re-render to clear it when it expires
    if (delay < 0 || (uint32_t)delay > rem) delay = (int)rem;
  }
  _display->endFrame();

  if (delay < 0) delay = 0;
  _next_render_at = now_ms + (uint32_t)delay;
  _has_rendered = true;
  _dirty = false;
}

}  // namespace mishmesh
