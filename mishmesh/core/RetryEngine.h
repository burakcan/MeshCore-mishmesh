// mishmesh/core/RetryEngine.h
#pragma once
#include <stdint.h>
#include <mishmesh/core/MessageStore.h>   // ConvoKey

namespace mishmesh {

// Actions the engine asks the host to perform. The host implements these with
// real I/O (re-transmit over the radio, flip a store record to failed); the
// engine itself stays pure so its decisions are host-testable.
struct RetryActions {
  virtual ~RetryActions() {}
  // Re-transmit a still-undelivered direct message. `attempt` is the retry
  // number (1..MAX_RETRIES). `resetPath` asks the host to clear the contact's
  // learned route first, so this attempt falls back to flood routing.
  virtual void retransmit(const ConvoKey& key, uint32_t senderTime,
                          uint8_t attempt, bool resetPath) = 0;
  // Give up on a message after the last retry - mark it failed.
  virtual void markFailed(const ConvoKey& key, uint32_t senderTime) = 0;
};

// Tracks on-device-sent direct messages that are still awaiting delivery and
// drives automatic re-transmission. The message store is the source of truth
// for "still pending"; each tick the host scans it and reports every pending
// DM via see(), then endScan() retransmits the ones whose deadline has passed
// and drops the ones that have since been delivered/failed/deleted.
//
// No heap: a fixed table of in-flight messages, populated only in see().
class RetryEngine {
public:
  static const int     MAX_PENDING       = 8;     // messages tracked at once
  static const uint8_t MAX_RETRIES        = 5;     // re-transmissions after the first send
  static const uint8_t RESET_PATH_AFTER   = 2;     // reset the route on this retry (when enabled)
  static const uint32_t RETRY_INTERVAL_MS = 8000;  // wait between attempts

  RetryEngine() { reset(); }
  void reset();

  void configure(bool autoRetry, bool autoResetPath) {
    _enabled = autoRetry; _resetPath = autoResetPath;
  }
  bool enabled() const { return _enabled; }

  // --- scan protocol: call once per tick ---
  void beginScan();                                          // mark all entries unseen
  void see(const ConvoKey& key, uint32_t senderTime, uint32_t now);  // a still-pending DM
  void endScan(uint32_t now, RetryActions& actions);         // reap + fire due retries

  // --- introspection (tests) ---
  int  trackedCount() const;
  bool attemptsFor(const ConvoKey& key, uint32_t senderTime, uint8_t& out) const;

private:
  struct Entry {
    bool     used;
    bool     seen;
    ConvoKey key;
    uint32_t senderTime;
    uint8_t  attempts;
    uint32_t dueAt;
  };
  Entry _e[MAX_PENDING];
  bool  _enabled;
  bool  _resetPath;

  int find(const ConvoKey& key, uint32_t senderTime) const;
};

}  // namespace mishmesh
