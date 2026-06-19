#pragma once

#include <stdint.h>

namespace mishmesh {

struct AppletStorage;

// What tick() just fired; also the ringing state the alert screen displays.
enum class ClockEvent : uint8_t { None = 0, TimerDone, AlarmDue };

// The clock engine behind the Clock applet: stopwatch, countdown timer, daily
// alarm, and the persisted world-clock city picks. Pure logic on caller-supplied
// clocks (a millisecond tick + the UTC epoch), so it is host-testable and keeps
// counting while its UI is backgrounded or closed - UITask::loop() ticks it, the
// applet only renders its state. Alarm/cities/timer-duration persist via
// AppletStorage ("alrm"/"wclk"/"tmrd"); begin(nullptr) is valid (defaults, no-op saves).
class ClockService {
public:
  static constexpr int MAX_LAPS = 8;
  static constexpr int MAX_CITIES = 4;
  static constexpr uint32_t TIMER_MAX_SECS = 8 * 3600;

  void begin(AppletStorage* st);

  // --- stopwatch (ms clock; wrap-safe for sessions < 49 days) ---
  bool     swRunning() const { return _swRunning; }
  uint32_t swElapsedMs(uint32_t nowMs) const { return _swAccum + (_swRunning ? nowMs - _swStart : 0); }
  void     swToggle(uint32_t nowMs);
  void     swReset();
  void     swLap(uint32_t nowMs);            // running only; keeps the newest MAX_LAPS
  int      swLapCount() const { return _lapTotal < MAX_LAPS ? _lapTotal : MAX_LAPS; }
  int      swLapTotal() const { return _lapTotal; }
  uint32_t swLapMs(int i) const;             // i=0 -> newest kept lap
  int      swLapNumber(int i) const;         // 1-based absolute number of kept lap i

  // --- countdown timer ---
  bool     tmRunning() const { return _tmRunning; }
  bool     tmPaused() const { return !_tmRunning && _tmRemainMs != _tmDurationSecs * 1000u; }
  uint32_t tmDurationSecs() const { return _tmDurationSecs; }
  void     tmSetDurationSecs(uint32_t secs);   // idle only; persists
  uint32_t tmRemainingMs(uint32_t nowMs) const;
  void     tmToggle(uint32_t nowMs);           // start / pause / resume
  void     tmReset();

  // --- daily alarm ---
  bool    alarmEnabled() const { return _alEnabled; }
  uint8_t alarmHour() const { return _alHour; }
  uint8_t alarmMinute() const { return _alMinute; }
  void    setAlarm(uint8_t hour, uint8_t minute, bool enabled, uint32_t epochUtc,
                   int16_t tzOffsetMin);       // persists; won't fire for the current minute
  // Minutes until it next rings (0..1439), or -1 when disabled/clock unset.
  int32_t alarmMinutesAway(uint32_t epochUtc, int16_t tzOffsetMin) const;

  // --- world clock (values index the WorldClock.h city table) ---
  int     cityCount() const { return _cityCount; }
  uint8_t cityAt(int i) const;
  bool    addCity(uint8_t idx);                // persists; false when full or duplicate
  void    removeCity(int i);                   // persists

  // --- ring tunes (indices into sound::clockToneId's list; raw bytes here so
  // the engine stays free of the sound layer - the picker validates range) ---
  uint8_t alarmToneIdx() const { return _alTone; }
  uint8_t timerToneIdx() const { return _tmTone; }
  void    setAlarmToneIdx(uint8_t i);   // persists
  void    setTimerToneIdx(uint8_t i);   // persists

  // --- ring volumes: 0 = follow the system volume, 1..3 = fixed Low/Mid/High
  // (a fixed level rings even when the system volume is dialed to Mute) ---
  uint8_t alarmVolume() const { return _alVol; }
  uint8_t timerVolume() const { return _tmVol; }
  void    setAlarmVolume(uint8_t v);    // persists
  void    setTimerVolume(uint8_t v);    // persists

  // --- ringing state (set by tick, cleared by the alert screen) ---
  ClockEvent ringing() const { return _ringing; }
  void       acknowledge() { _ringing = ClockEvent::None; }

  // Advance the engine; returns the event that just became due (edge, not level).
  ClockEvent tick(uint32_t nowMs, uint32_t epochUtc, int16_t tzOffsetMin);

  void resetForTest();

private:
  void persistAlarm();
  void persistCities();

  AppletStorage* _st = nullptr;

  bool     _swRunning = false;
  uint32_t _swAccum = 0;               // ms banked while stopped
  uint32_t _swStart = 0;               // tick at last start
  uint32_t _laps[MAX_LAPS] = {0};      // ring buffer of elapsed-at-lap
  int      _lapTotal = 0;              // absolute laps taken since reset

  bool     _tmRunning = false;
  uint32_t _tmDurationSecs = 300;
  uint32_t _tmRemainMs = 300 * 1000u;  // valid while not running
  uint32_t _tmEndAt = 0;               // tick deadline while running

  bool     _alEnabled = false;
  uint8_t  _alHour = 7, _alMinute = 0;
  uint32_t _alFiredStamp = 0;          // local epoch-minute last fired (once per minute)
  uint8_t  _alTone = 1;                // "Beeper" - the original alarm ring
  uint8_t  _tmTone = 0;                // "Arpeggio" - the original timer ring
  uint8_t  _alVol = 0;                 // 0 = system volume
  uint8_t  _tmVol = 0;

  uint8_t  _cities[MAX_CITIES] = {0};
  int      _cityCount = 0;

  ClockEvent _ringing = ClockEvent::None;
};

ClockService& clockService();

}  // namespace mishmesh
