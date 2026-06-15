// mishmesh/core/RetryEngine.cpp
#include <mishmesh/core/RetryEngine.h>

namespace mishmesh {

void RetryEngine::reset() {
  for (int i = 0; i < MAX_PENDING; i++) _e[i].used = false;
  _enabled = false;
  _resetPath = false;
}

int RetryEngine::find(const ConvoKey& key, uint32_t senderTime) const {
  for (int i = 0; i < MAX_PENDING; i++)
    if (_e[i].used && _e[i].senderTime == senderTime && _e[i].key.equals(key)) return i;
  return -1;
}

void RetryEngine::beginScan() {
  for (int i = 0; i < MAX_PENDING; i++) _e[i].seen = false;
}

void RetryEngine::see(const ConvoKey& key, uint32_t senderTime, uint32_t now) {
  int i = find(key, senderTime);
  if (i >= 0) { _e[i].seen = true; return; }
  for (int j = 0; j < MAX_PENDING; j++) {
    if (!_e[j].used) {
      _e[j].used = true; _e[j].seen = true;
      _e[j].key = key; _e[j].senderTime = senderTime;
      _e[j].attempts = 0; _e[j].dueAt = now + RETRY_INTERVAL_MS;
      return;
    }
  }
  // table full: ignore (best effort - older pending messages still get retried)
}

void RetryEngine::endScan(uint32_t now, RetryActions& actions) {
  for (int i = 0; i < MAX_PENDING; i++) {
    Entry& e = _e[i];
    if (!e.used) continue;
    if (!e.seen) { e.used = false; continue; }   // delivered/failed/deleted since last scan
    if (!_enabled) continue;                       // tracking only; no retries when off
    if ((int32_t)(now - e.dueAt) < 0) continue;    // not due yet (wrap-safe compare)

    if (e.attempts >= MAX_RETRIES) {               // out of retries -> fail
      actions.markFailed(e.key, e.senderTime);
      e.used = false;
      continue;
    }
    e.attempts++;
    bool reset = _resetPath && e.attempts == RESET_PATH_AFTER;
    actions.retransmit(e.key, e.senderTime, e.attempts, reset);
    e.dueAt = now + RETRY_INTERVAL_MS;
  }
}

int RetryEngine::trackedCount() const {
  int n = 0;
  for (int i = 0; i < MAX_PENDING; i++) if (_e[i].used) n++;
  return n;
}

bool RetryEngine::attemptsFor(const ConvoKey& key, uint32_t senderTime, uint8_t& out) const {
  int i = find(key, senderTime);
  if (i < 0) return false;
  out = _e[i].attempts;
  return true;
}

}  // namespace mishmesh
