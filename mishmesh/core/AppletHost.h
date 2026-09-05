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

  // Which face a bistable panel is left showing once it sleeps: an index into
  // the mishmesh/core/SleepScreen.h table, 0 (blank) by default. Ignored on a
  // panel that really powers down.
  void setSleepScreen(int idx) { _sleep_face = idx; }
  int  sleepScreen() const { return _sleep_face; }
  // Orientation choice for a face that reads either way (0 = Auto). A face that
  // only reads one way ignores this and gets that way applied for it.
  void setSleepOrientation(int choice) { _sleep_orient = choice; }
  int  sleepOrientation() const { return _sleep_orient; }
  // The rotation the user chose, so the panel can be put back after a sleep face
  // was painted some other way round. applyDisplayRotation() keeps this current;
  // the adapter seeds it at boot, where the driver is turned before the host exists.
  void setUiRotation(int quarters) { _ui_rotation = ((quarters % 4) + 4) % 4; }

  // A user wake this long after the panel blanked resets navigation to home;
  // shorter naps keep your place. 0 = always reset, WAKE_HOME_NEVER = never.
  void setWakeHomeMillis(uint32_t ms) { _wake_home_ms = ms; }
  // Sleep the panel now instead of waiting out the auto-off timer, e.g. engaging
  // the screen lock. Honoured on the next loop pass, not here: an applet asks for
  // this from onRender, and the frame it is in the middle of still has to flush
  // before the sleep face can replace it.
  void requestSleep() { _sleep_requested = true; }

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
  // Whether the screen lock is engaged, for anything drawing device state.
  bool deviceLocked() const;

  // Millisecond clock as of the current loop() pass. Lets an applet time input
  // gestures (onInput carries no timestamp) without reaching for Arduino millis().
  uint32_t nowMs() const { return _loop_now; }

  // When input was last seen, 0 before the first one. Feeds the unread
  // reminder's "any key stops the nagging" rule.
  uint32_t lastInputMs() const { return _last_input_ms; }

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
  // Change the driver's UI magnification and rebuild the canvas around the new
  // logical size. Applets keep their state; anything holding pixel geometry (list
  // scroll offsets) re-derives it on the next frame.
  void applyUiScale(int mult);
  bool displaySupportsUiScale() const;
  // Work the adapter needs serviced while a slow panel blocks the loop. Input is
  // drained here already; the sound sequencer is the other case, since a note
  // whose duration expires during a flush keeps sounding until the loop resumes.
  // It belongs to the adapter rather than to the host because servicing it needs
  // real time, and the host has no clock - _loop_now is the pre-render stamp.
  void setBusyHook(void (*fn)(void*), void* arg) { _busy_hook = fn; _busy_hook_arg = arg; }

  void applyDisplayRotation(int quarters);
  bool displaySupportsOrientation() const;
  // Quarter turns the screen has been rotated by, pushed down to every source so
  // the spatially-mounted ones turn with it. Sources added later pick it up too.
  void setInputRotation(int quarters);
  // Fixed offset between the control surface and the panel, which is a property
  // of how the board is built rather than anything the user chose. Composes with
  // setInputRotation, so "follow the screen" stays correct on a board whose stick
  // is not mounted square to its display.
  void setInputMountRotation(int quarters);
  int  inputRotation() const { return (_input_mount + _input_user) % 4; }

  // Test/inspection accessor: current toast text (empty if no toast has been posted).
  const char* toastForTest() const { return _toast_msg; }
  // Test/inspection accessor: current applet stack depth.
  int depthForTest() const { return _depth; }

  // Test/inspection accessor for the live held-button snapshot.
  const InputState& ctxInput() const { return _input_state; }

  // Test/inspection accessor: how many times the sleep face has been painted.
  int sleepPaintsForTest() const { return _sleep_paints; }

private:
  void renderIfDue(uint32_t now_ms);
  // Compose and flush the configured sleep face, arming the next repaint from
  // the delay it returns. Also spends the hourly ghost-clearing full refresh.
  void paintSleepFace(uint32_t now_ms);
  // Repaint the sleep face if it asked for one and its deadline has passed.
  void renderSleepFace(uint32_t now_ms);
  // Blank the panel and leave the sleep face on it. Shared by the auto-off
  // deadline and requestSleep().
  void enterSleep(uint32_t now_ms);
  // Turn the panel for a face that only reads one way, and put it back on wake.
  void applySleepRotation();
  void restoreSleepRotation();
  void drawBubble(uint32_t now_ms);   // top-right new-message badge overlay
#ifdef MISHMESH_INPUT_PROFILE
  // UI-responsiveness overlay (build with -D MISHMESH_INPUT_PROFILE). Draws the
  // loop-stall / render / input-drop counters over the foreground. See InputProfiler.
  void drawProfileOverlay();
  InputProfiler _prof;
  uint32_t _prof_last_paint = 0;   // last time the overlay forced a refresh
#endif
  // Push the foreground applet's input preferences (currently repeatMask())
  // to every source. Called after each foreground change.
  void applyInputContext();
  // Drain every input source once, dispatching what they report. Called both
  // before and after rendering each loop so a slow frame cannot open an
  // input-blind gap - see loop().
  void pumpInput(uint32_t now_ms);
  void handleReport(const InputReport& rep, uint32_t now_ms);
  // Whether a wake should reset navigation to home. Asks the whole stack, not
  // just the top: a keypad opened over a chat is foreground, but popping to root
  // would take the chat with it and throw away what was being typed.
  bool wakeResetsToHome(uint32_t now_ms) const;
  void pushInputRotation();   // mount + user, out to every source
  void rebuildCanvas();       // after the driver's logical size changes
  void refreshInputState();   // OR every source's heldMask() into _input_state
  static void busyPollThunk(const void* self);
  void pollDuringBusy();

  DisplayDriver* _display;
  Canvas _canvas;
  AppletContext _ctx;

  Applet* _stack[MAX_STACK];
  int _depth;

  InputSource* _sources[MAX_SOURCES];
  int _nsources;

  int  _input_mount = 0;            // see setInputMountRotation()
  int  _input_user = 0;             // see setInputRotation()

  void (*_busy_hook)(void*) = nullptr;
  void* _busy_hook_arg = nullptr;

  static const int BUSY_QUEUE = 8;
  InputReport _busyQueue[BUSY_QUEUE];
  uint8_t _busy_count;

  uint32_t _next_render_at;
  uint32_t _last_flush_ms;   // exclusive-mode flush-rate cap timestamp
  bool _has_rendered;
  bool _dirty;

  uint32_t _loop_now;   // now_ms of the current loop() pass; see nowMs()
  uint32_t _auto_off_ms;
  uint32_t _last_activity;
  bool _activity_init;
  // Separate from _last_activity: activity also advances while a blocksSleep()
  // applet holds the screen awake (see loop()), which is not input. The unread
  // reminder must not be dismissed just because a screen is staying on - see
  // lastInputMs().
  uint32_t _last_input_ms;

  uint32_t _slept_at;   // now_ms when auto-off last blanked the panel (0 = not slept)
  uint32_t _wake_home_ms;   // see setWakeHomeMillis()

  // Partial refreshes leave residue on e-ink and nothing else in the driver ever
  // clears it, so a face that repaints (the clock) would ghost its way through
  // the night. Spend one full refresh an hour, always while the panel is asleep
  // and its flash is not in anyone's way. A static face never repaints, so it
  // never reaches this and never pays for it.
  static const uint32_t SLEEP_FULL_REFRESH_MS = 3600000;
  int      _sleep_face;
  int      _sleep_orient;     // see setSleepOrientation()
  int      _ui_rotation;      // quarter turns the user chose
  bool     _sleep_rotated;    // panel is turned away from _ui_rotation for a face
  bool     _sleep_requested;  // see requestSleep()
  uint32_t _sleep_next_at;    // when the face wants its next repaint
  bool     _sleep_has_next;   // false once a face returns SLEEP_NEVER
  uint32_t _sleep_full_at;    // earliest next ghost-clearing full refresh
  int      _sleep_paints;

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
