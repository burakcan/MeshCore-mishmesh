#include <mishmesh/applets/ClockAlertApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const uint32_t REBEEP_MS = 4000;
static const uint32_t RING_OUT_MS = 60000;

void ClockAlertApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _app = ctx.app;
  _sound = ctx.sound;
}

void ClockAlertApplet::dismiss() {
  clockService().acknowledge();
  if (_sound) _sound->stop();
  if (_host && _host->foreground() == this) _host->pop();
}

int ClockAlertApplet::onRender(Canvas& c) {
  uint32_t now = c.now();
  if (_raisedAt == 0) _raisedAt = now;
  if (now - _raisedAt >= RING_OUT_MS) { dismiss(); return 0; }
  // Re-ring on a beat, but never restart a tune that is still playing (the
  // longer melodies outlast REBEEP_MS).
  if (_sound && !_sound->isPlaying() &&
      (_lastBeep == 0 || now - _lastBeep >= REBEEP_MS)) {
    _lastBeep = now;
    bool isAlarm = _kind == ClockEvent::AlarmDue;
    sound::SoundId id = sound::clockToneId(isAlarm ? clockService().alarmToneIdx()
                                                   : clockService().timerToneIdx());
    uint8_t vol = isAlarm ? clockService().alarmVolume() : clockService().timerVolume();
    if (vol) _sound->play(id, (sound::VolumeLevel)vol);   // fixed level, rings through Mute
    else     _sound->play(id);                            // follow the system volume
  }

  bool alarm = _kind == ClockEvent::AlarmDue;
  int w = c.width(), h = c.height();
  c.drawGlyph(iconFont(), w / 2 - 6, 4, (uint16_t)(alarm ? Icon::Bell : Icon::Clock),
              DisplayDriver::LIGHT);
  c.drawText(fontSubtitle(), w / 2, 19, alarm ? "Alarm" : "Timer done",
             DisplayDriver::LIGHT, TextAlign::Center);

  char buf[12] = "";
  bool fmt12 = _app && _app->timeFormat12h();
  if (alarm) {
    LocalTime lt{};
    lt.hour = clockService().alarmHour();
    lt.minute = clockService().alarmMinute();
    formatClock(buf, sizeof(buf), lt, fmt12);
  } else {
    uint32_t s = clockService().tmDurationSecs();
    if (s >= 3600) snprintf(buf, sizeof(buf), "%u:%02u:%02u", (unsigned)(s / 3600),
                            (unsigned)((s / 60) % 60), (unsigned)(s % 60));
    else snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  }
  // fontNum has digits/':' only; a 12h "7:30 PM" needs a face with letters.
  c.drawText((alarm && fmt12) ? fontSubtitle() : fontNum(), w / 2, 33, buf,
             DisplayDriver::LIGHT, TextAlign::Center);
  c.drawText(fontCaption(), w / 2, h - 7, "Press any key to dismiss",
             DisplayDriver::LIGHT, TextAlign::Center);
  return 250;
}

bool ClockAlertApplet::onInput(InputEvent) {
  dismiss();
  return true;   // swallow everything, including Back
}

ClockAlertApplet& clockAlertApplet() {
  static ClockAlertApplet a;
  return a;
}

}  // namespace mishmesh
