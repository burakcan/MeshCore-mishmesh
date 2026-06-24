// mishmesh/core/MsgTypes.h
// Shared types/constants for message storage. Included by MessageStore.h,
// MsgCodec.h, and ConvoIndex.h (instead of MessageStore.h directly) to
// break the circular include that would arise from MessageStore.h also
// including MsgCodec.h and ConvoIndex.h.
#pragma once
#include <stdint.h>
#include <string.h>

namespace mishmesh {

static const int MAX_CONVOS   = 64;
static const int PER_CHAT_CAP = 100;
static const int MAX_TRACKED  = 8;
static const int MAX_REPEATS  = 8;
static const int MAX_PATH     = 8;
static const int MAX_TEXT     = 160;
static const int PREVIEW_LEN  = 40;

enum MsgKind   : uint8_t { KIND_INBOUND = 0, KIND_OUT_DM = 1, KIND_OUT_CHAN = 2 };
enum MsgStatus : uint8_t { ST_PENDING = 0, ST_SENT = 1, ST_DELIVERED = 2, ST_FAILED = 3 };

struct ConvoKey {
  uint8_t type;
  uint8_t id[6];
  bool equals(const ConvoKey& o) const { return type == o.type && memcmp(id, o.id, 6) == 0; }
};
ConvoKey directKey(const uint8_t* pubkeyPrefix6);
ConvoKey channelKey(uint8_t channelIdx);

struct MsgRecord {
  ConvoKey       key;
  uint8_t        kind;
  uint32_t       senderTime;
  uint32_t       localTime;
  const char*    text;
  uint16_t       textLen;
  int8_t         snrx4;
  uint8_t        hops;
  const uint8_t* path;
  uint8_t        pathLen;
  uint8_t        status;
  uint16_t       tripTimeMs;
  uint8_t        heardCount;
  // LIFETIME: text and path point into MessageStore's shared _arena or _recBuf.
  // Valid only until the next getMessage() call on the same store instance.
};

struct ConvoSummary {
  ConvoKey key;
  uint32_t lastTime;
  uint16_t unread;
  uint16_t notifyUnread;
  uint16_t count;
  uint32_t logBytes;
  char     preview[PREVIEW_LEN];
};

struct RepeatRec {
  int8_t         snrx4;
  uint8_t        hops;
  const uint8_t* path;
  uint8_t        pathLen;
};

} // namespace mishmesh
