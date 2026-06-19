#include <mishmesh/sound/Sounds.h>

namespace mishmesh { namespace sound {

// Indexed by SoundId; const so it lands in flash. Direct access is correct on
// nRF52/ESP32 (memory-mapped flash) and on host.
static const SoundDef kSounds[(int)SoundId::COUNT] = {
  /* BootJingle     */ { SoundCategory::System,       "Startup",  "Startup:d=4,o=5,b=160:16c6,16e6,8g6" },
  /* ShutdownJingle */ { SoundCategory::System,       "Shutdown", "Shutdown:d=4,o=5,b=100:8g5,16e5,16c5" },
  /* MsgChime       */ { SoundCategory::Notification, "Standard", "NokiaStd:d=32,o=6,b=110:8a#,32p,8a#,4p,16p,8a#,32p,8a#" },
  /* MsgKerplop     */ { SoundCategory::Notification, "Droplet",  "kerplop:d=16,o=6,b=120:32g#,32c#" },
  /* MsgAck         */ { SoundCategory::Notification, "Ack",      "ack:d=32,o=8,b=120:c" },
  /* UiTick         */ { SoundCategory::Ui,           "Tick",     "tick:d=32,o=7,b=200:c" },
  /* UiConfirm      */ { SoundCategory::Ui,           "Confirm",  "ok:d=16,o=6,b=200:c,e" },
  /* UiError        */ { SoundCategory::Ui,           "Error",    "err:d=8,o=5,b=160:e,c" },
  /* MsgTritone     */ { SoundCategory::Notification, "Tri-tone", "tritone:d=16,o=6,b=160:c7,8g,8e" },
  /* MsgDing        */ { SoundCategory::Notification, "Ding",     "ding:d=8,o=7,b=140:c" },
  /* MsgChirp       */ { SoundCategory::Notification, "Chirp",    "chirp:d=32,o=7,b=180:g,c8" },
  /* MsgSms         */ { SoundCategory::Notification, "SMS",      "sms:d=16,o=7,b=280:c,16p,c,16p,c,8p.,8c.,16p,8c.,8p.,c,16p,c,16p,c" },  // morse ...--...
  /* MsgKnock       */ { SoundCategory::Notification, "Knock",    "knock:d=16,o=5,b=140:c,8p,c" },
  // System, not Notification: a timer/alarm the user armed must ring even with
  // message notifications muted. Master mute still silences them.
  /* TimerDone      */ { SoundCategory::System,       "Arpeggio", "timer:d=16,o=6,b=180:c,e,g,4c7,8p,c,e,g,4c7" },
  /* AlarmRing      */ { SoundCategory::System,       "Beeper",   "alarm:d=16,o=7,b=140:c,8p,c,8p,c,4p,c,8p,c,8p,c" },
  // Classic ring tunes for the alarm/timer picker. All public-domain melodies
  // (Tarrega, Westminster Quarters, Beethoven, the US bugle call).
  /* NokiaTune      */ { SoundCategory::System,       "Nokia",    "nokia:d=4,o=5,b=125:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a" },
  /* BigBen         */ { SoundCategory::System,       "Big Ben",  "bigben:d=4,o=5,b=112:e,c,d,2g,p,g,d,e,2c" },
  /* FurElise       */ { SoundCategory::System,       "Fur Elise", "furelise:d=8,o=5,b=125:e6,d#6,e6,d#6,e6,b,d6,c6,4a,p,c,e,a,4b,p,e,g#,b,4c6" },
  /* OdeToJoy       */ { SoundCategory::System,       "Ode to Joy", "odetojoy:d=4,o=5,b=140:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d" },
  /* Reveille       */ { SoundCategory::System,       "Reveille", "reveille:d=16,o=5,b=140:8g,c6,e6,8c6,8g,c6,e6,8c6,8g,c6,8e6,c6,8g,8e6,4c6" },
};

const SoundDef* soundDef(SoundId id) {
  if ((int)id >= (int)SoundId::COUNT) return nullptr;
  return &kSounds[(int)id];
}

// Picker display order. Index 0/1 reuse the two legacy notification sounds.
static const SoundId kNotifyTones[] = {
  SoundId::MsgChime,    // 0 Standard
  SoundId::MsgKerplop,  // 1 Droplet
  SoundId::MsgTritone,  // 2
  SoundId::MsgDing,     // 3
  SoundId::MsgChirp,    // 4
  SoundId::MsgSms,      // 5
  SoundId::MsgKnock,    // 6
};

int notifyToneCount() { return (int)(sizeof(kNotifyTones) / sizeof(kNotifyTones[0])); }

SoundId notifyToneId(int i) {
  if (i < 0 || i >= notifyToneCount()) i = 0;
  return kNotifyTones[i];
}

const char* notifyToneName(int i) {
  const SoundDef* d = soundDef(notifyToneId(i));
  return d ? d->name : "";
}

SoundId notifyTypeDefault(bool channel) {
  return channel ? SoundId::MsgKerplop : SoundId::MsgChime;
}

bool resolveNotifyTone(uint8_t chatByte, uint8_t typeByte, SoundId typeDefault, SoundId& out) {
  uint8_t b = chatByte ? chatByte : typeByte;   // chat override wins; 0 falls through
  if (b == NOTIFY_TONE_SILENT) return false;
  if (b == NOTIFY_TONE_DEFAULT) { out = typeDefault; return true; }
  int i = (int)b - NOTIFY_TONE_BASE;
  out = (i >= 0 && i < notifyToneCount()) ? notifyToneId(i) : typeDefault;
  return true;
}

// Alarm/timer picker order. Index 0/1 are the original rings, so a fresh device
// (stored index absent -> 0/1) keeps the existing sounds.
static const SoundId kClockTones[] = {
  SoundId::TimerDone,   // 0 "Arpeggio" (timer default)
  SoundId::AlarmRing,   // 1 "Beeper"   (alarm default)
  SoundId::NokiaTune,
  SoundId::BigBen,
  SoundId::FurElise,
  SoundId::OdeToJoy,
  SoundId::Reveille,
};

int clockToneCount() { return (int)(sizeof(kClockTones) / sizeof(kClockTones[0])); }

SoundId clockToneId(int i) {
  if (i < 0 || i >= clockToneCount()) i = 0;
  return kClockTones[i];
}

const char* clockToneName(int i) {
  const SoundDef* d = soundDef(clockToneId(i));
  return d ? d->name : "";
}

const char* notifyToneEncodedName(uint8_t encoded, bool perChat, SoundId typeDefault) {
  if (encoded == NOTIFY_TONE_SILENT) return "Silent";
  if (encoded == NOTIFY_TONE_DEFAULT) {
    if (perChat) return "Default";
    const SoundDef* d = soundDef(typeDefault);
    return d ? d->name : "Default";
  }
  return notifyToneName((int)encoded - NOTIFY_TONE_BASE);
}

}}  // namespace mishmesh::sound
