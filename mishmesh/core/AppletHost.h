#pragma once

#include <stdint.h>
#include <mishmesh/core/InputEvent.h>
#include <mishmesh/core/InputSource.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/Canvas.h>

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
  // Swap the foreground applet for another without changing stack depth: the current
  // top is stopped and `a` takes its place, so a later Back pops to whatever was
  // underneath (not back to the replaced applet). Used to hand off between sibling
  // drill-ins, e.g. discovery-detail -> contact-detail after adding a contact.
  void replace(Applet* a);

  Applet* foreground() const;
  int depth() const { return _depth; }

  void dispatch(InputEvent ev, bool repeat = false);
  void loop(uint32_t now_ms);

  // Transient feedback drawn over the foreground for ~1.4s. Outlives applet
  // pops (e.g. "Contact deleted" shown after a detail screen closes itself).
  void postToast(const char* msg);

private:
  void renderIfDue(uint32_t now_ms);
  // Push the foreground applet's input preferences (currently wantsBackRepeat())
  // to every source. Called after each foreground change.
  void applyInputContext();
  // Drain every input source once, dispatching (and bounce-coalescing) what they
  // report. Called both before and after rendering each loop so a slow frame can't
  // open an input-blind gap - see loop().
  void pumpInput(uint32_t now_ms);

  DisplayDriver* _display;
  Canvas _canvas;
  AppletContext _ctx;

  Applet* _stack[MAX_STACK];
  int _depth;

  InputSource* _sources[MAX_SOURCES];
  int _nsources;

  uint32_t _next_render_at;
  bool _has_rendered;
  bool _dirty;

  uint32_t _auto_off_ms;
  uint32_t _last_activity;
  bool _activity_init;

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
};

}  // namespace mishmesh
