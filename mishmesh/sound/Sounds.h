#pragma once
#include "SoundEngine.h"

namespace mishmesh { namespace sound {

enum class SoundId : uint8_t {
  BootJingle = 0, ShutdownJingle,
  MsgChime, MsgKerplop, MsgAck,     // named, selectable notification ringtones
  UiTick, UiConfirm, UiError,
  MsgTritone, MsgDing, MsgChirp, MsgSms, MsgKnock,   // [mishmesh] added notification tones
  COUNT
};

struct SoundDef {
  SoundCategory category;
  const char*   name;    // human-readable (for the ringtone chooser)
  const char*   rtttl;
};

const SoundDef* soundDef(SoundId id);   // nullptr if out of range

// Selectable notification ringtones, in picker display order. The stored tone
// index (encoded byte - NOTIFY_TONE_BASE) indexes this list.
int          notifyToneCount();
SoundId      notifyToneId(int i);          // i in [0,count); out of range -> first
const char*  notifyToneName(int i);        // == soundDef(notifyToneId(i))->name
SoundId      notifyTypeDefault(bool channel);  // firmware default tone per message type

// 1-byte selection encoding, shared by per-type (NodePrefs) and per-chat storage.
static const uint8_t NOTIFY_TONE_DEFAULT = 0;   // unset -> fall through / firmware default
static const uint8_t NOTIFY_TONE_SILENT  = 1;   // play nothing
static const uint8_t NOTIFY_TONE_BASE    = 2;   // 2+i -> ringtone index i

// Resolve the effective tone: per-chat override wins; a 0 byte falls through to
// the per-type default; a 0 there means the firmware default for the type.
// Returns false when the result is Silent (caller skips the beep).
bool resolveNotifyTone(uint8_t chatByte, uint8_t typeByte, SoundId typeDefault, SoundId& out);

// Display label for an encoded byte. perChat: 0 -> "Default". !perChat: 0 -> the
// type default tone's name. 1 -> "Silent". Else the tone's name.
const char* notifyToneEncodedName(uint8_t encoded, bool perChat, SoundId typeDefault);

}}  // namespace mishmesh::sound
