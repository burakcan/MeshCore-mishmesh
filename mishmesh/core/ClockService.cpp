#include <mishmesh/core/ClockService.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/WorldClock.h>

namespace mishmesh {

void ClockService::begin(AppletStorage* st) {
  _st = st;
  if (!_st) return;
  uint8_t buf[1 + MAX_CITIES];
  if (_st->load("alrm", buf, 3) == 3 && buf[1] < 24 && buf[2] < 60) {
    _alEnabled = buf[0] != 0;
    _alHour = buf[1];
    _alMinute = buf[2];
  }
  uint8_t n = _st->load("wclk", buf, sizeof(buf));
  if (n >= 1) {
    _cityCount = 0;
    int want = buf[0] < MAX_CITIES ? buf[0] : MAX_CITIES;
    for (int i = 0; i < want && 1 + i < n; i++) {
      if (buf[1 + i] < worldCityCount()) _cities[_cityCount++] = buf[1 + i];
    }
  }
  if (_st->load("altn", buf, 1) == 1) _alTone = buf[0];
  if (_st->load("tmtn", buf, 1) == 1) _tmTone = buf[0];
  if (_st->load("alvl", buf, 1) == 1 && buf[0] <= 3) _alVol = buf[0];
  if (_st->load("tmvl", buf, 1) == 1 && buf[0] <= 3) _tmVol = buf[0];
  if (_st->load("tmrd", buf, 2) == 2) {
    uint32_t secs = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8);
    if (secs >= 10 && secs <= TIMER_MAX_SECS) {
      _tmDurationSecs = secs;
      _tmRemainMs = secs * 1000u;
    }
  }
}

// --- stopwatch ---

void ClockService::swToggle(uint32_t nowMs) {
  if (_swRunning) {
    _swAccum += nowMs - _swStart;
    _swRunning = false;
  } else {
    _swStart = nowMs;
    _swRunning = true;
  }
}

void ClockService::swReset() {
  _swRunning = false;
  _swAccum = 0;
  _lapTotal = 0;
}

void ClockService::swLap(uint32_t nowMs) {
  if (!_swRunning) return;
  _laps[_lapTotal % MAX_LAPS] = swElapsedMs(nowMs);
  _lapTotal++;
}

uint32_t ClockService::swLapMs(int i) const {
  int kept = swLapCount();
  if (i < 0 || i >= kept) return 0;
  return _laps[(_lapTotal - 1 - i) % MAX_LAPS];
}

int ClockService::swLapNumber(int i) const {
  int kept = swLapCount();
  if (i < 0 || i >= kept) return 0;
  return _lapTotal - i;
}

// --- timer ---

void ClockService::tmSetDurationSecs(uint32_t secs) {
  if (_tmRunning) return;
  if (secs < 10) secs = 10;
  if (secs > TIMER_MAX_SECS) secs = TIMER_MAX_SECS;
  _tmDurationSecs = secs;
  _tmRemainMs = secs * 1000u;
  if (_st) {
    uint8_t b[2] = { (uint8_t)(secs & 0xFF), (uint8_t)(secs >> 8) };
    _st->save("tmrd", b, 2);
  }
}

uint32_t ClockService::tmRemainingMs(uint32_t nowMs) const {
  if (!_tmRunning) return _tmRemainMs;
  int32_t left = (int32_t)(_tmEndAt - nowMs);
  return left > 0 ? (uint32_t)left : 0;
}

void ClockService::tmToggle(uint32_t nowMs) {
  if (_tmRunning) {
    _tmRemainMs = tmRemainingMs(nowMs);
    _tmRunning = false;
  } else {
    if (_tmRemainMs == 0) _tmRemainMs = _tmDurationSecs * 1000u;
    _tmEndAt = nowMs + _tmRemainMs;
    _tmRunning = true;
  }
}

void ClockService::tmReset() {
  _tmRunning = false;
  _tmRemainMs = _tmDurationSecs * 1000u;
}

// --- alarm ---

void ClockService::setAlarm(uint8_t hour, uint8_t minute, bool enabled,
                            uint32_t epochUtc, int16_t tzOffsetMin) {
  _alHour = hour < 24 ? hour : 0;
  _alMinute = minute < 60 ? minute : 0;
  _alEnabled = enabled;
  // Arm for the NEXT occurrence: enabling an alarm during its own minute must
  // not ring instantly, so mark the current local minute as already fired.
  uint32_t localEpoch = epochUtc + (int32_t)tzOffsetMin * 60;
  uint32_t localMin = localEpoch / 60;
  bool dueNow = (localMin % 1440) == (uint32_t)_alHour * 60 + _alMinute;
  _alFiredStamp = (enabled && epochUtc && dueNow) ? localMin : 0;
  persistAlarm();
}

int32_t ClockService::alarmMinutesAway(uint32_t epochUtc, int16_t tzOffsetMin) const {
  if (!_alEnabled || epochUtc == 0) return -1;
  uint32_t localEpoch = epochUtc + (int32_t)tzOffsetMin * 60;
  int32_t nowMin = (int32_t)((localEpoch % 86400) / 60);
  int32_t target = (int32_t)_alHour * 60 + _alMinute;
  return ((target - nowMin) % 1440 + 1440) % 1440;
}

void ClockService::setAlarmToneIdx(uint8_t i) {
  _alTone = i;
  if (_st) _st->save("altn", &i, 1);
}

void ClockService::setTimerToneIdx(uint8_t i) {
  _tmTone = i;
  if (_st) _st->save("tmtn", &i, 1);
}

void ClockService::setAlarmVolume(uint8_t v) {
  _alVol = v <= 3 ? v : 0;
  if (_st) _st->save("alvl", &_alVol, 1);
}

void ClockService::setTimerVolume(uint8_t v) {
  _tmVol = v <= 3 ? v : 0;
  if (_st) _st->save("tmvl", &_tmVol, 1);
}

void ClockService::persistAlarm() {
  if (!_st) return;
  uint8_t b[3] = { (uint8_t)(_alEnabled ? 1 : 0), _alHour, _alMinute };
  _st->save("alrm", b, 3);
}

// --- world clock ---

uint8_t ClockService::cityAt(int i) const {
  return (i >= 0 && i < _cityCount) ? _cities[i] : 0;
}

bool ClockService::addCity(uint8_t idx) {
  if (_cityCount >= MAX_CITIES || idx >= worldCityCount()) return false;
  for (int i = 0; i < _cityCount; i++)
    if (_cities[i] == idx) return false;
  _cities[_cityCount++] = idx;
  persistCities();
  return true;
}

void ClockService::removeCity(int i) {
  if (i < 0 || i >= _cityCount) return;
  for (int j = i; j < _cityCount - 1; j++) _cities[j] = _cities[j + 1];
  _cityCount--;
  persistCities();
}

void ClockService::persistCities() {
  if (!_st) return;
  uint8_t b[1 + MAX_CITIES];
  b[0] = (uint8_t)_cityCount;
  for (int i = 0; i < _cityCount; i++) b[1 + i] = _cities[i];
  _st->save("wclk", b, (uint8_t)(1 + _cityCount));
}

// --- tick ---

ClockEvent ClockService::tick(uint32_t nowMs, uint32_t epochUtc, int16_t tzOffsetMin) {
  if (_tmRunning && (int32_t)(nowMs - _tmEndAt) >= 0) {
    _tmRunning = false;
    _tmRemainMs = _tmDurationSecs * 1000u;   // re-arm for the next run
    _ringing = ClockEvent::TimerDone;
    return ClockEvent::TimerDone;
  }
  if (_alEnabled && epochUtc != 0) {
    uint32_t localMin = (epochUtc + (int32_t)tzOffsetMin * 60) / 60;
    bool due = (localMin % 1440) == (uint32_t)_alHour * 60 + _alMinute;
    if (due && _alFiredStamp != localMin) {
      _alFiredStamp = localMin;
      _ringing = ClockEvent::AlarmDue;
      return ClockEvent::AlarmDue;
    }
  }
  return ClockEvent::None;
}

void ClockService::resetForTest() {
  _st = nullptr;
  _swRunning = false; _swAccum = 0; _swStart = 0; _lapTotal = 0;
  _tmRunning = false; _tmDurationSecs = 300; _tmRemainMs = 300 * 1000u; _tmEndAt = 0;
  _alEnabled = false; _alHour = 7; _alMinute = 0; _alFiredStamp = 0;
  _alTone = 1; _tmTone = 0; _alVol = 0; _tmVol = 0;
  _cityCount = 0;
  _ringing = ClockEvent::None;
}

ClockService& clockService() {
  static ClockService s;
  return s;
}

}  // namespace mishmesh
