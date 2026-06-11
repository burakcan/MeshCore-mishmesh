#include <mishmesh/sound/Sounds.h>

namespace mishmesh { namespace sound {

// Indexed by SoundId; const so it lands in flash. Direct access is correct on
// nRF52/ESP32 (memory-mapped flash) and on host.
static const SoundDef kSounds[(int)SoundId::COUNT] = {
  /* BootJingle     */ { SoundCategory::System,       "Startup",  "Startup:d=4,o=5,b=160:16c6,16e6,8g6" },
  /* ShutdownJingle */ { SoundCategory::System,       "Shutdown", "Shutdown:d=4,o=5,b=100:8g5,16e5,16c5" },
  /* MsgChime       */ { SoundCategory::Notification, "Chime",    "MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7" },
  /* MsgKerplop     */ { SoundCategory::Notification, "Kerplop",  "kerplop:d=16,o=6,b=120:32g#,32c#" },
  /* MsgAck         */ { SoundCategory::Notification, "Ack",      "ack:d=32,o=8,b=120:c" },
  /* UiTick         */ { SoundCategory::Ui,           "Tick",     "tick:d=32,o=7,b=200:c" },
  /* UiConfirm      */ { SoundCategory::Ui,           "Confirm",  "ok:d=16,o=6,b=200:c,e" },
  /* UiError        */ { SoundCategory::Ui,           "Error",    "err:d=8,o=5,b=160:e,c" },
};

const SoundDef* soundDef(SoundId id) {
  if ((int)id >= (int)SoundId::COUNT) return nullptr;
  return &kSounds[(int)id];
}

}}  // namespace mishmesh::sound
