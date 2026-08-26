#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ClockService.h>
#include <mishmesh/widgets/TabBar.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

// The Clock app: Stopwatch / Timer / Pomodoro / Alarm / World clock / Settings tabs.
// All timekeeping lives in ClockService (ticked from UITask::loop), so the
// stopwatch and timer keep running when this applet is closed; this class only
// renders that state and routes input. The Settings tab embeds the shared
// timeSettings() panel, same as the other tabbed applets.
class ClockApplet : public Applet {
public:
  ClockApplet() : Applet("Clock") {}

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // A running stopwatch holds the screen on (you're watching it tick) and stays
  // put on wake. A countdown timer counts down in the background and rings via
  // ClockAlertApplet, so it must NOT block sleep - but if you wake mid-countdown,
  // stay on it rather than resetting to home. An idle tab goes home like anything else.
  // Pomodoro follows the timer's rule, not the stopwatch's: you focus with the
  // screen off and it rings on each phase boundary via the alert.
  bool blocksSleep() const override {
    return _tab == TAB_STOPWATCH && clockService().swRunning();
  }
  bool keepOnWake() const override {
    return (_tab == TAB_STOPWATCH && clockService().swRunning()) ||
           (_tab == TAB_TIMER && (clockService().tmRunning() || clockService().tmPaused())) ||
           (_tab == TAB_POMODORO && clockService().pmActive());
  }

  // test seams
  int  selectedTabForTest() const { return _tab; }
  bool editingAlarmForTest() const { return _editor.open && !_editor.forTimer; }
  bool editingTimerForTest() const { return _editor.open && _editor.forTimer; }
  bool pickingCityForTest() const { return _pickingCity; }
  bool pomodoroSetupOpenForTest() const { return _pomoSetupOpen; }

private:
  enum Tab : int { TAB_STOPWATCH, TAB_TIMER, TAB_POMODORO, TAB_ALARM, TAB_WORLD, TAB_SETTINGS };

  bool settingsTab() const { return _tab == TAB_SETTINGS; }
  int  renderStopwatch(Canvas& c, int y, int h);
  int  renderTimer(Canvas& c, int y, int h);
  int  renderAlarm(Canvas& c, int y, int h);
  int  renderWorld(Canvas& c, int y, int h);
  int  renderPomodoroIdle(Canvas& c, int y, int h);
  int  renderPomodoroRunning(Canvas& c, int y, int h);
  void drawSessionRing(Canvas& c, int cx, int cy, int r);   // C2 session ring for the running screen
  void openPomodoroSetup();
  void openAlarmEditor();
  void openTimerEditor();
  void drawEditor(Canvas& c);
  bool inputEditor(InputEvent ev);
  bool inputStopwatch(InputEvent ev);
  bool inputTimer(InputEvent ev);
  bool inputAlarm(InputEvent ev);
  bool inputWorld(InputEvent ev);
  bool inputPomodoro(InputEvent ev);

  AppletHost*  _host = nullptr;
  AppServices* _app = nullptr;
  TabBar   _tabs;
  int      _tab = 0;
  uint32_t _now = 0;            // last frame tick, for input-time toggles

  struct AlarmModel : ListModel {
    AppServices* app = nullptr;
    enum Row : int { Time, Enabled };
    int count() const override { return 2; }
    const char* label(int i) const override { return i == Time ? "Time" : "Enabled"; }
    const char* value(int i) const override;
    bool isToggle(int i) const override { return i == Enabled; }
    bool toggleState(int) const override { return clockService().alarmEnabled(); }
  } _alarmModel;
  ListMenu _alarmList;

  // Modal field editor shared by the alarm (HH:MM, or HH:MM AM/PM in 12h mode)
  // and the timer (H:MM:SS). NavLeft/NavRight move between fields, NavUp/NavDown
  // step (wrapping), Select commits, Back cancels.
  struct TimeEditor {
    const char* title = "";
    int  nFields = 0;
    int  vals[4] = {0};
    int  mins[4] = {0};   // inclusive lower bound per field (12h hours start at 1)
    int  maxs[4] = {0};   // exclusive wrap bound per field
    int  ampmField = -1;  // index of the AM/PM field (12h alarm), -1 = none
    int  sel = 0;
    bool open = false;
    bool forTimer = false;
  } _editor;

  struct WorldModel : ListModel {
    AppServices* app = nullptr;
    bool hasAddRow() const { return clockService().cityCount() < ClockService::MAX_CITIES; }
    int count() const override { return clockService().cityCount() + (hasAddRow() ? 1 : 0); }
    const char* label(int i) const override;
    uint16_t    icon(int i) const override;
    const char* value(int i) const override;
  } _worldModel;
  ListMenu _worldList;

  struct PickModel : ListModel {
    int count() const override;
    const char* label(int i) const override;
    const char* value(int i) const override;   // "UTC+05:30"
  } _pickModel;
  ListMenu _pickList;
  bool     _pickingCity = false;

  struct PomoSetupModel : ListModel {
    enum Row : int { Focus, Short, Long, SetN, Auto, ROW_COUNT };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override;
    const char* value(int i) const override;
    bool isToggle(int i) const override { return i == Auto; }
    bool toggleState(int) const override { return clockService().pmAutoAdvance(); }
  } _pomoSetupModel;
  ListMenu      _pomoSetupList;
  StepperDialog _pomoStepper;
  bool          _pomoSetupOpen = false;
  int           _pomoStepRow = -1;   // which setup row the stepper is editing, -1 = none
};

ClockApplet& clockApplet();

}  // namespace mishmesh
