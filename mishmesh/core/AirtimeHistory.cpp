#include <mishmesh/core/AirtimeHistory.h>

namespace mishmesh {

static uint16_t addSat(uint16_t a, uint32_t b) {
  uint32_t s = (uint32_t)a + b;
  return s > 0xFFFFu ? 0xFFFFu : (uint16_t)s;
}

void AirtimeHistory::reset() {
  for (int i = 0; i < BUCKETS; i++) { _tx[i] = 0; _rx[i] = 0; }
  _head = 0;
  _primed = false;
  _bucketStart = 0;
  _lastTx = 0;
  _lastRx = 0;
}

bool AirtimeHistory::advanceTo(uint32_t nowMs) {
  uint32_t elapsed = nowMs - _bucketStart;
  if (elapsed < BUCKET_MS) return false;
  uint32_t steps = elapsed / BUCKET_MS;
  if (steps >= (uint32_t)BUCKETS) {
    // Gap longer than the whole window (e.g. long sleep): everything retained is
    // stale. Clear and restart the clock at now.
    for (int i = 0; i < BUCKETS; i++) { _tx[i] = 0; _rx[i] = 0; }
    _head = 0;
    _bucketStart = nowMs;
    return true;
  }
  for (uint32_t s = 0; s < steps; s++) {
    _head = (uint8_t)((_head + 1) % BUCKETS);
    _tx[_head] = 0;
    _rx[_head] = 0;
  }
  _bucketStart += steps * BUCKET_MS;
  return false;
}

void AirtimeHistory::tick(uint32_t nowMs, uint32_t totalTx, uint32_t totalRx) {
  if (!_primed) {
    // First sample only establishes the baseline; we can't know how much of the
    // lifetime totals happened in this minute vs. before, so bank nothing yet.
    _primed = true;
    _bucketStart = nowMs;
    _lastTx = totalTx;
    _lastRx = totalRx;
    return;
  }

  // Roll the ring forward to the current minute first, so this sample's airtime
  // is attributed to now rather than the minute it started in.
  bool wiped = advanceTo(nowMs);

  // Guard counter resets (resetStats / reboot with a live sampler): a total that
  // went backwards means the baseline is gone, so skip this delta rather than
  // banking a huge bogus spike.
  uint32_t dTx = totalTx >= _lastTx ? totalTx - _lastTx : 0;
  uint32_t dRx = totalRx >= _lastRx ? totalRx - _lastRx : 0;
  _lastTx = totalTx;
  _lastRx = totalRx;

  // A window-length gap already discarded stale history; the delta accumulated
  // across it is unattributable, so re-baseline without banking it.
  if (wiped) return;

  _tx[_head] = addSat(_tx[_head], dTx);
  _rx[_head] = addSat(_rx[_head], dRx);
}

uint16_t AirtimeHistory::txBucket(int i) const {
  if (i < 0 || i >= BUCKETS) return 0;
  return _tx[(_head + 1 + i) % BUCKETS];
}

uint16_t AirtimeHistory::rxBucket(int i) const {
  if (i < 0 || i >= BUCKETS) return 0;
  return _rx[(_head + 1 + i) % BUCKETS];
}

uint16_t AirtimeHistory::maxBucket(bool combined) const {
  uint32_t m = 1;
  for (int i = 0; i < BUCKETS; i++) {
    uint32_t v = combined ? (uint32_t)_tx[i] + _rx[i] : _tx[i];
    if (!combined && _rx[i] > v) v = _rx[i];   // max of tx or rx when not stacked
    if (v > m) m = v;
  }
  return m > 0xFFFFu ? 0xFFFFu : (uint16_t)m;
}

uint32_t AirtimeHistory::txWindowMs() const {
  uint32_t s = 0;
  for (int i = 0; i < BUCKETS; i++) s += _tx[i];
  return s;
}

uint32_t AirtimeHistory::rxWindowMs() const {
  uint32_t s = 0;
  for (int i = 0; i < BUCKETS; i++) s += _rx[i];
  return s;
}

}  // namespace mishmesh
