#pragma once
#include "SoundEngine.h"

namespace mishmesh { namespace sound {

enum class SoundId : uint8_t {
  BootJingle = 0, ShutdownJingle,
  MsgChime, MsgKerplop, MsgAck,     // named, selectable notification ringtones
  UiTick, UiConfirm, UiError,
  COUNT
};

struct SoundDef {
  SoundCategory category;
  const char*   name;    // human-readable (for a future ringtone chooser)
  const char*   rtttl;
};

const SoundDef* soundDef(SoundId id);   // nullptr if out of range

}}  // namespace mishmesh::sound
