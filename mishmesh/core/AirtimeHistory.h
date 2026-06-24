#pragma once

#include <stdint.h>

namespace mishmesh {

// A fixed-size ring buffer of per-minute TX/RX airtime, sampled from the
// Dispatcher's cumulative counters. The Dispatcher exposes only lifetime totals
// (getTotalAirTime/getReceiveAirTime) with no history, so the adapter ticks this
// once per loop and we bank the deltas into 1-minute buckets. Sampling lives in
// the loop (not the applet) so history keeps accruing while the screen is off.
//
// No heap: the buffers are fixed members. Sixty 1-minute buckets = the last hour,
// which lines up with the default duty-cycle window.
class AirtimeHistory {
public:
  static const int      BUCKETS   = 60;
  static const uint32_t BUCKET_MS = 60000;   // one minute

  AirtimeHistory() { reset(); }

  void reset();

  // Fold the newest cumulative totals in. nowMs is a millis()-style clock;
  // totalTx/totalRx are the Dispatcher's monotonic airtime counters (ms). Safe to
  // call every loop - cheap, and only advances buckets when a minute has elapsed.
  void tick(uint32_t nowMs, uint32_t totalTx, uint32_t totalRx);

  int      bucketCount() const { return BUCKETS; }
  uint32_t bucketMs()    const { return BUCKET_MS; }

  // Chronological access: i=0 is the oldest retained bucket, i=BUCKETS-1 is the
  // current (still-filling) one. Values are airtime-ms within that minute.
  uint16_t txBucket(int i) const;
  uint16_t rxBucket(int i) const;

  // Largest single-bucket value across the window, for auto-scaling a graph.
  // combined=true takes the max of (tx+rx) per bucket. Never returns 0 (clamped
  // to 1) so callers can divide without guarding.
  uint16_t maxBucket(bool combined) const;

  // Sum of airtime over all retained buckets - "used in the last hour".
  uint32_t txWindowMs() const;
  uint32_t rxWindowMs() const;

  bool primed() const { return _primed; }

private:
  // Advance the ring to nowMs, zeroing each minute crossed. Returns true if the
  // gap exceeded the whole window (everything wiped) so the caller re-baselines.
  bool advanceTo(uint32_t nowMs);

  uint16_t _tx[BUCKETS];
  uint16_t _rx[BUCKETS];
  uint8_t  _head;          // index of the current (newest) bucket
  bool     _primed;        // seen at least one sample (baseline established)
  uint32_t _bucketStart;   // nowMs at which _head's minute began
  uint32_t _lastTx;        // last observed cumulative totals
  uint32_t _lastRx;
};

}  // namespace mishmesh
