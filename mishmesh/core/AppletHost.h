#pragma once

#include <stdint.h>
#include <mishmesh/core/InputEvent.h>
#include <mishmesh/core/InputSource.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/Canvas.h>
#ifdef MISHMESH_INPUT_PROFILE
#include <mishmesh/core/InputProfiler.h>
#endif

// Max display flushes/sec while a wantsExclusive() applet is foreground. The sim
// (onRender) still runs every loop pass; only the blocking panel flush is capped,
// to bound the I2C stall duty cycle so inbound mesh packets aren't starved.
// Override per-build with -D MISHMESH_GAME_MAX_FLUSH_FPS=<n>.
#ifndef MISHMESH_GAME_MAX_FLUSH_FPS
#define MISHMESH_GAME_MAX_FLUSH_FPS 60
#endif

namespace mishmesh {

// The UI runtime: owns a fixed-size applet stack (root at index 0), the input
// sources, the Canvas, and render scheduling. No allocation after construction.
class AppletHost {
public:
  static const int MAX_STACK = 8;
  static const int MAX_SOURCES = 4;

  AppletHost(DisplayDriver* display, const AppletContext& ctx);

  void addSource(InputSource* src);

  // Blank the panel after this many ms without input (0 disables). OLED-safe:
  // rendering does not extend the deadline, only input does.
  void setAutoOffMillis(uint32_t ms) { _auto_off_ms = ms; }

  void setRoot(Applet* root);
  void push(Applet* a);
  void pop();
  // Pop every drill-in back to the root (home). No-op when already at root.
  void popToRoot();
  // Swap the foreground applet for another without changing stack depth: the current
  // top is stopped and `a` takes its place, so a later Back pops to whatever was
  // underneath (not back to the replaced applet). Used to hand off between sibling
  // drill-ins, e.g. discovery-detail -> contact-detail after adding a contact.
  void replace(Applet* a);

  Applet* foreground() const;
  int depth() const { return _depth; }

  // Millisecond clock as of the current loop() pass. Lets an applet time input
  // gestures (onInput carries no timestamp) without reaching for Arduino millis().
  uint32_t nowMs() const { return _loop_now; }

  void dispatch(InputEvent ev, bool repeat = false);
  void loop(uint32_t now_ms);

  // Transient feedback drawn over the foreground for ~1.4s. Outlives applet
  // pops (e.g. "Contact deleted" shown after a detail screen closes itself).
  void postToast(const char* msg);

  // A small message badge that slides in at the top-right corner for ~2s, over
  // whatever applet is foreground. Used for low-priority new-message alerts when
  // the user is busy in some other screen. `unread` is shown beside the glyph.
  void postBubble(uint16_t unread);

  // Display power, for the notification router: wake the panel without delivering
  // an input event (an incoming message can wake the screen to show a banner).
  bool isDisplayOn() const;
  void wakeDisplay();
  // Force a repaint on the next loop (e.g. after state changed outside an applet).
  void requestRender() { _dirty = true; }

  // Test/inspection accessor for the live held-button snapshot.
  const InputState& ctxInput() const { return _input_state; }

private:
  void renderIfDue(uint32_t now_ms);
  void drawBubble(uint32_t now_ms);   // top-right new-message badge overlay
#ifdef MISHMESH_INPUT_PROFILE
  // UI-responsiveness overlay (build with -D MISHMESH_INPUT_PROFILE). Draws the
  // loop-stall / render / input-drop counters over the foreground. See InputProfiler.
  void drawProfileOverlay();
  InputProfiler _prof;
  uint32_t _prof_last_paint = 0;   // last time the overlay forced a refresh
#endif
  // Push the foreground applet's input preferences (currently wantsBackRepeat())
  // to every source. Called after each foreground change.
  void applyInputContext();
  // Drain every input source once, dispatching (and bounce-coalescing) what they
  // report. Called both before and after rendering each loop so a slow frame can't
  // open an input-blind gap - see loop().
  void pumpInput(uint32_t now_ms);
  void refreshInputState();   // OR every source's heldMask() into _input_state

  DisplayDriver* _display;
  Canvas _canvas;
  AppletContext _ctx;

  Applet* _stack[MAX_STACK];
  int _depth;

  InputSource* _sources[MAX_SOURCES];
  int _nsources;

  uint32_t _next_render_at;
  uint32_t _last_flush_ms;   // exclusive-mode flush-rate cap timestamp
  bool _has_rendered;
  bool _dirty;

  uint32_t _loop_now;   // now_ms of the current loop() pass; see nowMs()
  uint32_t _auto_off_ms;
  uint32_t _last_activity;
  bool _activity_init;

  // A user wake this long after the panel blanked resets navigation to home
  // (unless the foreground opts out via keepOnWake). Short naps keep your place.
  static const uint32_t WAKE_HOME_THRESHOLD_MS = 60000;
  uint32_t _slept_at;   // now_ms when auto-off last blanked the panel (0 = not slept)

  // Contact-bounce coalescing: a noisy joystick press can emit the same event
  // twice within a few ms (multi-click debounce is off for snappy nav). Drop a
  // repeat of the same event inside this window - far below the human repeat rate.
  static const uint32_t INPUT_DEBOUNCE_MS = 60;
  InputEvent _last_input_event;
  uint32_t _last_input_ms;
  bool _input_seen;

  char _toast_msg[28];
  uint32_t _toast_until;
  bool _toast_pending;

  uint32_t _bubble_start;     // stamped when the bubble first shows
  uint32_t _bubble_until;     // bubble cleared once now passes this
  bool     _bubble_pending;   // posted during dispatch; stamped on the next loop
  uint16_t _bubble_unread;

  InputState _input_state;
};

}  // namespace mishmesh
