// mishmesh/core/ConvoIndex.h
// RAM chat-list model: summaries + previews + per-chat byte totals.
// Independent of message bodies; serializes to a compact index blob.
#pragma once
#include <stdint.h>
#include "MsgTypes.h"   // ConvoKey, ConvoSummary (split out to avoid circular deps)

namespace mishmesh {

class ConvoIndex {
public:
  ConvoIndex() { reset(); }
  void reset();

  // ---- slot access ----
  int  find(const ConvoKey& key) const;
  int  count() const { return _count; }
  // Ensure a slot exists; on overflow evicts the LRU slot (caller handles log
  // deletion via the returned victim key from evictLRU). Returns a ref to the slot.
  ConvoSummary& ensure(const ConvoKey& key);
  // Remove the least-recently-active slot, report its key. Returns false if empty.
  bool evictLRU(ConvoKey& victimOut);
  // Peek at the LRU key without removing the slot. Caller must check count() > 0 first.
  ConvoKey lruKey() const;
  // Recency-sorted (newest first); same algorithm as MessageStore::getConvo.
  bool get(int rankIndex, ConvoSummary& out) const;
  // Remove a slot outright; used by deleteConvo (Task 11). False if absent.
  bool dropByKey(const ConvoKey& key);

  // ---- touch / timing ----
  // Update lastTime for slot ci; ensures it is always strictly ahead of all
  // others so the touched convo sorts first regardless of clock skew.
  void touch(int ci, uint32_t time);

  // ---- preview ----
  void        setPreview(const ConvoKey& key, const char* text, uint16_t len);
  const char* preview(const ConvoKey& key) const;

  // ---- accessors used by MessageStore ----
  uint16_t rawCount(int ci) const;   // message count of slot ci
  uint16_t totalUnread() const;
  uint16_t totalNotifyUnread() const;
  uint32_t totalLogBytes() const;    // sum of logBytes across all convos (for budget check)

  // ---- persistence ----
  // serialize: writes "MIDX" magic + version 1 + per-convo packed records.
  // Returns bytes written, 0 if cap too small.
  uint32_t serialize(uint8_t* out, uint32_t cap) const;
  // deserialize: validates magic/version/count; false on bad header or truncation.
  bool deserialize(const uint8_t* in, uint32_t len);

private:
  ConvoSummary _convos[MAX_CONVOS];
  int          _count;
};

} // namespace mishmesh
