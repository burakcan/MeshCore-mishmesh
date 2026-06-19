#include <mishmesh/applets/settings/TimeSettingsPanel.h>
#include <mishmesh/applets/SetTimeApplet.h>
#include <mishmesh/applets/SoundPickerApplet.h>
#include <mishmesh/core/Actions.h>   // volumeLevelName
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ClockService.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/sound/Sounds.h>
#include <stdio.h>

namespace mishmesh {

// Stepper works in quarter-hour units; render as UTC±HH:MM.
static void tzStepLabel(int qh, char* out, uint16_t cap) {
  formatOffset(out, cap, (int16_t)(qh * 15));
}

// Date-format stepper renders each option as a real example date. The sample is
// stashed here because StepperFormatFn is a context-free function pointer.
static LocalTime s_dfSample;
static void dateFmtStepLabel(int v, char* out, uint16_t cap) {
  formatDate(out, cap, s_dfSample, (DateFormat)v);
}

const char* TimeSettingsPanel::Model::label(int i) const {
  static const char* LABELS[] = { "Time zone", "Time format", "Date format",
                                  "Alarm sound", "Alarm volume",
                                  "Timer sound", "Timer volume",
                                  "Set automatically", "Set date & time" };
  return (i >= 0 && i < 9) ? LABELS[i] : "";
}

// 0 = follow the shared sound volume; 1..3 = fixed level (rings through Mute).
static const char* ringVolumeName(uint8_t v) {
  return v == 0 ? "System" : volumeLevelName(v);
}

const char* TimeSettingsPanel::Model::value(int i) const {
  if (!app) return nullptr;
  static char buf[24];
  if (i == TimeZone) { formatOffset(buf, sizeof(buf), app->tzOffsetMinutes()); return buf; }
  if (i == TimeFmt)  { snprintf(buf, sizeof(buf), "%s", app->timeFormat12h() ? "12h" : "24h"); return buf; }
  if (i == DateFmt) {   // live example rendered in the chosen format
    LocalTime lt = applyTz(app->epochSeconds(), app->tzOffsetMinutes());
    formatDate(buf, sizeof(buf), lt, (DateFormat)app->dateFormat());
    return buf;
  }
  if (i == AlarmSound)  return sound::clockToneName(clockService().alarmToneIdx());
  if (i == TimerSound)  return sound::clockToneName(clockService().timerToneIdx());
  if (i == AlarmVolume) return ringVolumeName(clockService().alarmVolume());
  if (i == TimerVolume) return ringVolumeName(clockService().timerVolume());
  if (i == SetTime) {
    LocalTime lt = applyTz(app->epochSeconds(), app->tzOffsetMinutes());
    formatDate(buf, sizeof(buf), lt, (DateFormat)app->dateFormat());
    return buf;
  }
  return nullptr;   // SetAuto renders as a toggle pill
}

void TimeSettingsPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _snd = ctx.sound;
  _model.app = _app;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();   // singleton reuse: setModel skips reset on same-ptr rebind
  _editingTz = false;
}

void TimeSettingsPanel::onShow() {
  // Row count depends on auto/manual; nothing cached, so no action needed beyond
  // letting the list re-read the model. Kept for symmetry / future refresh.
}

int TimeSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  if (_editingTz || _editingDateFmt) { _stepper.draw(c, 0, 0, c.width(), c.height()); return 100; }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool TimeSettingsPanel::onInput(InputEvent ev) {
  if (_editingTz || _editingDateFmt) {
    if (_stepper.onInput(ev)) {
      StepperResult r = _stepper.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _app) {
          if (_editingTz) _app->setTzOffsetMinutes((int16_t)(_stepper.value() * 15));
          else            _app->setDateFormat((uint8_t)_stepper.value());
        }
        _editingTz = _editingDateFmt = false;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _app) {
    int i = _list.selected();
    if (i == Model::TimeZone) {
      _stepper.configure("Time zone", _app->tzOffsetMinutes() / 15, -48, 56, tzStepLabel);
      _editingTz = true;
    } else if (i == Model::TimeFmt) {
      _app->setTimeFormat12h(!_app->timeFormat12h());
    } else if (i == Model::DateFmt) {
      uint32_t now = _app->epochSeconds();
      if (now) {
        s_dfSample = applyTz(now, _app->tzOffsetMinutes());
      } else {   // clock unset: use a day>12 sample so D/M vs M/D is unambiguous
        LocalTime s{}; s.year = 2026; s.month = 12; s.day = 31; s_dfSample = s;
      }
      _stepper.configure("Date format", _app->dateFormat(), 0,
                         (int)DateFormat::COUNT - 1, dateFmtStepLabel);
      _editingDateFmt = true;
    } else if (i == Model::AlarmSound || i == Model::TimerSound) {
      bool alarm = i == Model::AlarmSound;
      soundPickerApplet().setClock(alarm, alarm ? "Alarm sound" : "Timer sound");
      if (_host) _host->push(&soundPickerApplet());
    } else if (i == Model::AlarmVolume || i == Model::TimerVolume) {
      // Cycle System -> Low -> Mid -> High in place, previewing the ring.
      bool alarm = i == Model::AlarmVolume;
      ClockService& cs = clockService();
      uint8_t v = ((alarm ? cs.alarmVolume() : cs.timerVolume()) + 1) & 3;
      if (alarm) cs.setAlarmVolume(v); else cs.setTimerVolume(v);
      if (_snd) {
        sound::SoundId id = sound::clockToneId(alarm ? cs.alarmToneIdx() : cs.timerToneIdx());
        if (v) _snd->play(id, (sound::VolumeLevel)v);
        else   _snd->play(id);
      }
    } else if (i == Model::SetAuto) {
      _app->setAutoTimeSync(!_app->autoTimeSync());
      // count may shrink to 3; keep selection in range.
      if (_list.selected() >= _model.count()) _list.setSelected(_model.count() - 1);
    } else if (i == Model::SetTime) {   // only reachable in manual mode
      setTimeApplet().configure(_app);
      if (_host) _host->push(&setTimeApplet());
    }
    return true;
  }
  return false;   // Back bubbles
}

TimeSettingsPanel& timeSettings() {
  static TimeSettingsPanel s;
  return s;
}

}  // namespace mishmesh
