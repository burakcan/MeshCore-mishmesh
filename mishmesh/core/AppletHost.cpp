#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/InputSource.h>
#include <mishmesh/text/Fonts.h>
#include <helpers/ui/DisplayDriver.h>
#include <string.h>
#include <stdio.h>
#ifdef MISHMESH_INPUT_PROFILE
#include <Arduino.h>   // millis() for sub-render frame timing
#endif

namespace mishmesh {

static const uint32_t TOAST_MS = 1400;

// Top-right new-message bubble: slides in, holds, slides back out.
static const uint32_t BUBBLE_MS      = 2200;
static const uint32_t BUBBLE_SLIDE_MS = 160;
static const uint32_t BUBBLE_TICK_MS  = 40;   // animation cadence while sliding

AppletHost::AppletHost(DisplayDriver* display, const AppletContext& ctx)
    : _display(display), _canvas(display), _ctx(ctx),
      _depth(0), _nsources(0),
      _next_render_at(0), _last_flush_ms(0), _has_rendered(false), _dirty(true),
      _auto_off_ms(30000), _last_activity(0), _activity_init(false),
      _last_input_event(InputEvent::None), _last_input_ms(0), _input_seen(false),
      _toast_until(0), _toast_pending(false),
      _bubble_start(0), _bubble_until(0), _bubble_pending(false), _bubble_unread(0) {
  for (int i = 0; i < MAX_STACK; i++) _stack[i] = nullptr;
  for (int i = 0; i < MAX_SOURCES; i++) _sources[i] = nullptr;
  _toast_msg[0] = 0;
  _ctx.host = this;
  _ctx.inputState = &_input_state;
}

void AppletHost::postToast(const char* msg) {
  strncpy(_toast_msg, msg ? msg : "", sizeof(_toast_msg) - 1);
  _toast_msg[sizeof(_toast_msg) - 1] = 0;
  _toast_pending = true;   // stamped with a deadline on the next loop (needs now)
  _dirty = true;
}

void AppletHost::postBubble(uint16_t unread) {
  _bubble_unread = unread;
  _bubble_pending = true;   // stamped with a deadline on the next loop (needs now)
  _dirty = true;
}

bool AppletHost::isDisplayOn() const {
  return _display != nullptr && _display->isOn();
}

void AppletHost::wakeDisplay() {
  if (_display != nullptr && !_display->isOn()) _display->turnOn();
  // Restart the auto-off window so the panel we just woke isn't blanked again on
  // the next loop: clearing _activity_init makes loop() restamp _last_activity to
  // the current now (same path used at first boot).
  _activity_init = false;
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
#ifdef MISHMESH_INPUT_PROFILE
      if (!rep.repeat) _prof.recordPolled();   // a discrete press edge was sampled
#endif
      if (_display != nullptr && !_display->isOn()) {
        _display->turnOn();   // first press only wakes; it isn't delivered
        _dirty = true;
      } else {
        bool bounce = _input_seen && rep.event == _last_input_event &&
                      now_ms - _last_input_ms < INPUT_DEBOUNCE_MS;
        _last_input_event = rep.event; _last_input_ms = now_ms; _input_seen = true;
        if (!bounce) {
#ifdef MISHMESH_INPUT_PROFILE
          if (!rep.repeat) _prof.recordDispatched();   // survived bounce coalescing
#endif
          dispatch(rep.event, rep.repeat);
        }
      }
    }
  }
}

void AppletHost::refreshInputState() {
  uint16_t held = 0;
  for (int i = 0; i < _nsources; i++) {
    if (_sources[i] != nullptr) held |= _sources[i]->heldMask();
  }
  _input_state.held = held;
}

void AppletHost::loop(uint32_t now_ms) {
  if (!_activity_init) {
    _last_activity = now_ms;
    _activity_init = true;
  }

#ifdef MISHMESH_INPUT_PROFILE
  _prof.tick(now_ms);
  // Repaint the overlay a few times a second even when the UI is otherwise idle,
  // so the live counters keep updating while you press.
  if (now_ms - _prof_last_paint >= 300) { _prof_last_paint = now_ms; _dirty = true; }
#endif

  pumpInput(now_ms);

  refreshInputState();   // [add] live held-button snapshot for real-time applets

  if (_toast_pending) {            // applet posted during input dispatch; stamp it now
    _toast_until = now_ms + TOAST_MS;
    _toast_pending = false;
    _dirty = true;
  }

  if (_bubble_pending) {           // posted from notify() (mesh callback); stamp it now
    _bubble_start = now_ms;
    _bubble_until = now_ms + BUBBLE_MS;
    _bubble_pending = false;
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

  bool exclusive = fg->wantsExclusive();
  bool due = exclusive || _dirty || !_has_rendered || now_ms >= _next_render_at;
  if (!due) return;

#ifdef MISHMESH_INPUT_PROFILE
  uint32_t _pt0 = millis();   // frame compose+flush cost, measured around the whole draw
#endif
  _canvas.setNow(now_ms);
  // Themed background - except in game mode, whose raw 1bpp blits bypass the
  // theme and expect the panel cleared dark. Drivers may ignore startFrame's
  // bkg (SH1106 clears black unconditionally), so in light mode paint the
  // background ourselves through the themed canvas.
  _display->startFrame(exclusive ? DisplayDriver::DARK : themedColor(DisplayDriver::DARK));
  if (!exclusive && themedColor(DisplayDriver::DARK) == DisplayDriver::LIGHT)
    _canvas.fillRect(0, 0, _canvas.width(), _canvas.height(), DisplayDriver::DARK);
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
  if (now_ms < _bubble_until) {
    drawBubble(now_ms);
    uint32_t elapsed = now_ms - _bubble_start;
    uint32_t remaining = _bubble_until - now_ms;
    uint32_t wake;
    if (elapsed < BUBBLE_SLIDE_MS)            wake = BUBBLE_TICK_MS;                  // sliding in
    else if (remaining > BUBBLE_SLIDE_MS)     wake = remaining - BUBBLE_SLIDE_MS;     // hold, then wake to slide out
    else                                      wake = BUBBLE_TICK_MS;                  // sliding out (+ final clear)
    if (delay < 0 || (uint32_t)delay > wake) delay = (int)wake;
  }
  if (exclusive) {
    uint32_t interval = 1000u / MISHMESH_GAME_MAX_FLUSH_FPS;
    if (!_has_rendered || (uint32_t)(now_ms - _last_flush_ms) >= interval) {
      _display->endFrame();
      _last_flush_ms = now_ms;
    }
    _next_render_at = now_ms;   // always due next pass; returned delay ignored
  } else {
#ifdef MISHMESH_INPUT_PROFILE
    drawProfileOverlay();
#endif
    _display->endFrame();
#ifdef MISHMESH_INPUT_PROFILE
    _prof.recordRender(millis() - _pt0);
#endif
    if (delay < 0) delay = 0;
    _next_render_at = now_ms + (uint32_t)delay;
  }
  _has_rendered = true;
  _dirty = false;
}

#ifdef MISHMESH_INPUT_PROFILE
// S<maxStall> R<maxRender> B<blindGaps>  /  p<polled> d<dispatched>, ms. Compare
// p (edges sampled) and d (edges acted on) against your own press count: fingers>p
// = starvation/hardware (watch B climb), p>d = software bounce ate it. See
// InputProfiler for the full triage.
void AppletHost::drawProfileOverlay() {
  const mf_font_s* f = fontBody();
  int fh = _canvas.fontHeight(f); if (fh <= 0) fh = 8;
  int w = _canvas.width();
  int bh = fh * 2 + 3;

  char l1[24], l2[24];
  snprintf(l1, sizeof(l1), "S%lu R%lu B%lu", (unsigned long)_prof.maxStall,
           (unsigned long)_prof.maxRender, (unsigned long)_prof.blindGaps);
  snprintf(l2, sizeof(l2), "p%lu d%lu", (unsigned long)_prof.polled,
           (unsigned long)_prof.dispatched);

  _canvas.fillRect(0, 0, w, bh, DisplayDriver::DARK);
  _canvas.drawText(f, 1, 1, l1, DisplayDriver::LIGHT);
  _canvas.drawText(f, 1, 1 + fh + 1, l2, DisplayDriver::LIGHT);
}
#endif

void AppletHost::drawBubble(uint32_t now_ms) {
  const mf_font_s* icon = iconFont();
  const mf_font_s* num  = fontBody();

  char cnt[6];
  if (_bubble_unread > 0) snprintf(cnt, sizeof(cnt), "%u", (unsigned)_bubble_unread);
  else cnt[0] = 0;

  const int iconW = 12, pad = 3, gap = 2, bh = 15;
  int glyphH = _canvas.fontHeight(icon); if (glyphH <= 0) glyphH = 12;
  int cntW = cnt[0] ? _canvas.textWidth(num, cnt) : 0;
  int bw = iconW + (cnt[0] ? gap + cntW : 0) + 2 * pad;
  int baseX = _canvas.width() - 1 - bw;   // 1px margin from the right edge
  const int by = 1;                       // 1px margin from the top edge

  // Slide in from off the right edge, hold, then slide back out the same way.
  uint32_t elapsed = now_ms - _bubble_start;
  uint32_t remaining = _bubble_until - now_ms;
  int travel = bw + 2;
  int slide = 0;
  if (elapsed < BUBBLE_SLIDE_MS)
    slide = (int)((uint32_t)travel * (BUBBLE_SLIDE_MS - elapsed) / BUBBLE_SLIDE_MS);
  else if (remaining < BUBBLE_SLIDE_MS)
    slide = (int)((uint32_t)travel * (BUBBLE_SLIDE_MS - remaining) / BUBBLE_SLIDE_MS);
  int bx = baseX + slide;

  _canvas.fillRect(bx, by, bw, bh, DisplayDriver::DARK);       // mask the content behind
  _canvas.drawRoundRect(bx, by, bw, bh, DisplayDriver::LIGHT);
  _canvas.drawGlyph(icon, bx + pad, by + (bh - glyphH) / 2,
                    (uint16_t)Icon::Message, DisplayDriver::LIGHT);
  if (cnt[0])
    _canvas.drawText(num, bx + pad + iconW + gap,
                     by + (bh - _canvas.fontHeight(num)) / 2, cnt, DisplayDriver::LIGHT);
}

}  // namespace mishmesh
