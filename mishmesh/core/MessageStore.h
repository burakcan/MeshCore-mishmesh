// mishmesh/core/MessageStore.h
#pragma once
#include <stdint.h>
#include <string.h>

namespace mishmesh {

static const int ARENA_BYTES  = 6144;
static const int MAX_CONVOS   = 64;
static const int PER_CHAT_CAP = 30;
static const int MAX_TRACKED  = 8;
static const int MAX_REPEATS  = 8;
static const int MAX_PATH     = 8;     // max path hash bytes stored per record/repeat
static const int MAX_TEXT     = 160;

enum MsgKind   : uint8_t { KIND_INBOUND = 0, KIND_OUT_DM = 1, KIND_OUT_CHAN = 2 };
enum MsgStatus : uint8_t { ST_PENDING = 0, ST_SENT = 1, ST_DELIVERED = 2, ST_FAILED = 3 };

struct ConvoKey {
  uint8_t type;       // 0 = direct, 1 = channel
  uint8_t id[6];      // direct: pubkey prefix; channel: id[0] = channel index
  bool equals(const ConvoKey& o) const { return type == o.type && memcmp(id, o.id, 6) == 0; }
};
ConvoKey directKey(const uint8_t* pubkeyPrefix6);
ConvoKey channelKey(uint8_t channelIdx);

// Read-only view into a stored message. Pointers are valid until the next
// store mutation. snrx4/hops/path/pathLen are meaningful for inbound; status/
// tripTimeMs for outbound DM; heardCount for outbound channel.
struct MsgRecord {
  ConvoKey       key;
  uint8_t        kind;          // MsgKind
  uint32_t       senderTime;
  uint32_t       localTime;
  const char*    text;          // not null-terminated; use textLen
  uint16_t       textLen;
  int8_t         snrx4;
  uint8_t        hops;
  const uint8_t* path;
  uint8_t        pathLen;
  uint8_t        status;        // MsgStatus (outbound)
  uint16_t       tripTimeMs;    // outbound DM when delivered
  uint8_t        heardCount;    // outbound channel
};

struct ConvoSummary {
  ConvoKey key;
  uint32_t lastTime;
  uint16_t unread;
  uint8_t  count;
};

struct RepeatRec {
  int8_t         snrx4;
  uint8_t        hops;
  const uint8_t* path;
  uint8_t        pathLen;       // last hop (repeater) = path[pathLen-1]
};

class MessageStore {
public:
  MessageStore() { reset(); }
  void reset();

  // ---- capture ----
  void appendInbound(const ConvoKey& key, const char* text, uint16_t textLen,
                     uint32_t senderTime, uint32_t recvTime,
                     int8_t snrx4, const uint8_t* path, uint8_t pathLen);
  void appendOutboundDM(const ConvoKey& key, const char* text, uint16_t textLen,
                        uint32_t senderTime, uint32_t sendTime,
                        uint32_t expectedAck, uint32_t sentMillis);
  void appendOutboundChannel(const ConvoKey& key, const char* text, uint16_t textLen,
                             uint32_t senderTime, uint32_t sendTime);
  void markDelivered(uint32_t expectedAck, uint16_t tripTimeMs);
  void addRepeat(const ConvoKey& key, uint32_t senderTime,
                 int8_t snrx4, const uint8_t* path, uint8_t pathLen);

  // ---- queries ----
  int  convoCount() const;                              // recency-sorted (newest first)
  bool getConvo(int index, ConvoSummary& out) const;
  uint16_t totalUnread() const;
  int  messageCount(const ConvoKey& key) const;
  bool getMessage(const ConvoKey& key, int index, MsgRecord& out) const;  // index 0 = oldest
  void setActiveConvo(const ConvoKey& key);
  void clearActiveConvo();

  int  repeatCount(const ConvoKey& key, int msgIndex) const;              // 0 if summary-only
  bool getRepeat(const ConvoKey& key, int msgIndex, int r, RepeatRec& out) const;

  void deleteMessage(const ConvoKey& key, int index);
  void clearConvo(const ConvoKey& key);

  uint32_t seq() const { return _seq; }

  // ---- persistence (no file I/O here; caller does the file) ----
  size_t serialize(uint8_t* out, size_t cap) const;     // returns bytes written, 0 if cap too small
  bool   deserialize(const uint8_t* in, size_t len);     // false on bad magic/version

private:
  // arena holds packed records, see MessageStore.cpp for the byte layout
  uint8_t  _arena[ARENA_BYTES];
  uint32_t _used;          // bytes used (live + tombstoned) from arena[0..]
  uint32_t _deadBytes;     // tombstoned bytes pending compaction
  uint32_t _seq;

  ConvoSummary _convos[MAX_CONVOS];
  int          _convoCount;

  struct Tracked {
    bool     used;
    ConvoKey key;
    uint32_t senderTime;     // correlates to the arena record
    uint8_t  kind;
    uint32_t expectedAck;    // DM
    RepeatRec repeats[MAX_REPEATS];
    uint8_t  rptStore[MAX_REPEATS][MAX_PATH];  // backing for RepeatRec.path
    uint8_t  rptCount;
  };
  Tracked _tracked[MAX_TRACKED];

  bool _hasActive;
  ConvoKey _active;

  // helpers (implemented across tasks)
  int  findConvo(const ConvoKey& key) const;
  int  ensureConvo(const ConvoKey& key);
  void touchConvo(int ci, uint32_t time);
  uint32_t recordSize(const uint8_t* rec) const;
  void compact();
  bool reserve(uint32_t need);              // ensure `need` free bytes (evict/compact)
  void evictOldestOfLargest();
  void dropConvoIfEmpty(int ci);
  void tombstoneOldestOf(const ConvoKey& key);
  Tracked* openTracked(const ConvoKey& key, uint32_t senderTime, uint8_t kind);
  Tracked* findTracked(const ConvoKey& key, uint32_t senderTime);
};

} // namespace mishmesh
