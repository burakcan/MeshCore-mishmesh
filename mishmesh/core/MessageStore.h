// mishmesh/core/MessageStore.h
#pragma once
// Shared type definitions - split to MsgTypes.h so MsgCodec.h and
// ConvoIndex.h can include them without creating a circular dependency
// with this file.
#include "MsgTypes.h"
#include "MsgCodec.h"
#include "ConvoIndex.h"
#include "MsgLogBackend.h"

namespace mishmesh {

class MessageStore {
public:
  static const uint32_t MSG_FLASH_BUDGET = 1200u * 1024;  // global log budget in bytes

  MessageStore() { reset(); }
  void reset();
  // Wire up the backend before use. Loads the saved index from the backend;
  // falls back to rebuildIndex() if the index blob is absent or corrupt.
  void begin(MsgLogBackend* b);
  // Persist the in-RAM index to the backend's index blob.
  void saveIndex();

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
  void markFailed(const ConvoKey& key, uint32_t senderTime);
  void updateExpectedAck(const ConvoKey& key, uint32_t senderTime, uint32_t newAck);
  void addRepeat(const ConvoKey& key, uint32_t senderTime,
                 int8_t snrx4, const uint8_t* path, uint8_t pathLen);

  // ---- queries ----
  int  convoCount() const;                              // recency-sorted (newest first)
  bool getConvo(int index, ConvoSummary& out) const;
  uint16_t totalUnread() const;
  uint16_t totalNotifyUnread() const;
  int  messageCount(const ConvoKey& key) const;
  bool getMessage(const ConvoKey& key, int index, MsgRecord& out) const;  // index 0 = oldest
  // Streams this chat's messages in order (0 = oldest) in ONE forward pass over
  // the flash log - O(n) reads total, vs O(n^2) for n separate getMessage() calls.
  // The record passed to cb is valid only until cb returns (shared scratch buffer).
  void forEachMessage(const ConvoKey& key,
                      void (*cb)(void* ctx, int index, const MsgRecord& rec),
                      void* ctx) const;
  void setActiveConvo(const ConvoKey& key);
  void clearActiveConvo();
  bool activeConvo(ConvoKey& out) const;
  bool lastInbound(ConvoKey& out) const;

  int  repeatCount(const ConvoKey& key, int msgIndex) const;
  bool getRepeat(const ConvoKey& key, int msgIndex, int r, RepeatRec& out) const;

  int  pendingDMCount() const;
  bool getPendingDM(int index, ConvoKey& outKey, uint32_t& outSenderTime) const;
  // Single-pass collection of up to `cap` still-pending outbound DMs (key +
  // senderTime), returning the count written. One walk over all logs - unlike
  // pendingDMCount()+getPendingDM() which re-walk every file per index (O(n^2)
  // flash opens). Used by the auto-retry scan, which runs every few seconds.
  int  collectPendingDMs(ConvoKey* outKeys, uint32_t* outTimes, int cap) const;
  int  getDMText(const ConvoKey& key, uint32_t senderTime, char* buf, int cap) const;

  void deleteMessage(const ConvoKey& key, int index);
  void clearConvo(const ConvoKey& key);    // empties messages, keeps the chat in the list
  void deleteConvo(const ConvoKey& key);   // empties messages AND removes the chat
  void markUnread(const ConvoKey& key);
  void markNotifiable(const ConvoKey& key);
  void ensureChannel(uint8_t channelIdx);

  uint32_t seq() const { return _seq; }

private:
  MsgLogBackend* _backend;
  ConvoIndex     _index;
  uint32_t       _seq;

  // Decode a packed record at `r` into `out` (out.text/path point into `r`).
  void decodeRecord(const uint8_t* r, const ConvoKey& key, MsgRecord& out) const;

  // Open-chat window: holds the tail records of the active convo that fit WINDOW_BYTES.
  // getMessage() zero-copies from here for recent messages, pages older ones via _recBuf.
  static const int WINDOW_BYTES = 5120;
  mutable uint8_t  _arena[WINDOW_BYTES];
  mutable ConvoKey _windowKey;
  mutable bool     _windowValid;
  mutable uint16_t _winOffs[PER_CHAT_CAP];  // in-arena offsets for each windowed record
  mutable int      _winCount;               // records in window
  mutable int      _winFirst;              // full-sequence index of first windowed record
  mutable int      _winTotal;              // total live records in the chat

  mutable uint8_t _recBuf[codec::MAX_REC]; // scratch for single-record reads (paging)
  // Word-aligned: LittleFS programs/reads this blob directly from here on a large
  // single transfer, and the nRF52840 QSPI DMA rejects a non-word-aligned RAM source.
  // Without alignas, the odd-sized _recBuf leaves _idxBuf unaligned and every index
  // write silently fails (wrote=0) - the on-device unread-count persistence bug.
  alignas(uint32_t) uint8_t _idxBuf[4096];   // serialised ConvoIndex (MIDX blob, ~3911 bytes max)

  struct Tracked {
    bool     used;
    ConvoKey key;
    uint32_t senderTime;
    uint8_t  kind;
    uint32_t expectedAck;
    RepeatRec repeats[MAX_REPEATS];
    uint8_t  rptStore[MAX_REPEATS][MAX_PATH_BYTES];
    uint8_t  rptCount;
  };
  Tracked _tracked[MAX_TRACKED];

  bool _hasActive;
  ConvoKey _active;
  bool _hasLastInbound;
  ConvoKey _lastInbound;

  Tracked* openTracked(const ConvoKey& key, uint32_t senderTime, uint8_t kind);
  Tracked* findTracked(const ConvoKey& key, uint32_t senderTime);

  // Evict LRU chat(s) until there's room for `need` bytes; returns false if the only
  // candidate to evict is `key` itself (hopeless - drop the incoming message).
  bool ensureSpace(const ConvoKey& key, uint32_t need);

  ConvoSummary& ensureSlot(const ConvoKey& key);
  void rebuildIndex();
  void rotateIfNeeded(const ConvoKey& key, ConvoSummary& c);

  // Window management
  void loadWindow(const ConvoKey& key) const;
  void invalidateWindow(const ConvoKey& key);

  // Streaming helper: find byte offset+len of the nth live record
  bool findRecordOffset(const char* name, int targetIdx,
                        uint32_t& offOut, uint32_t& lenOut) const;
};

} // namespace mishmesh
