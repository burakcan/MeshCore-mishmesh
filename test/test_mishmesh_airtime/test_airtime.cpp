#include <gtest/gtest.h>
#include <mishmesh/core/AirtimeHistory.h>

using namespace mishmesh;

static const uint32_t MIN = AirtimeHistory::BUCKET_MS;   // one minute
static const int N = AirtimeHistory::BUCKETS;

// The first tick only establishes the baseline: lifetime totals may include
// airtime from before we started sampling, so nothing is banked.
TEST(AirtimeHistory, FirstTickPrimesBaseline) {
  AirtimeHistory h;
  EXPECT_FALSE(h.primed());
  h.tick(1000, 5000, 20000);   // large lifetime totals on first sight
  EXPECT_TRUE(h.primed());
  EXPECT_EQ(0u, h.txWindowMs());
  EXPECT_EQ(0u, h.rxWindowMs());
}

// Deltas within the same minute accumulate into the current (newest) bucket.
TEST(AirtimeHistory, DeltaBanksIntoCurrentBucket) {
  AirtimeHistory h;
  h.tick(1000, 0, 0);
  h.tick(1000, 50, 10);
  h.tick(1000, 80, 30);
  EXPECT_EQ(80u, h.txWindowMs());
  EXPECT_EQ(30u, h.rxWindowMs());
  EXPECT_EQ((uint16_t)80, h.txBucket(N - 1));   // current = last chronological slot
  EXPECT_EQ((uint16_t)0,  h.txBucket(0));        // oldest still empty
}

// A minute rollover advances to a fresh bucket; the previous minute is retained
// separately, one slot back in chronological order.
TEST(AirtimeHistory, RolloverSeparatesMinutes) {
  AirtimeHistory h;
  h.tick(1000, 0, 0);
  h.tick(1000, 80, 0);          // minute 0
  h.tick(1000 + MIN, 100, 0);   // crosses into minute 1: +20 banked, then advance
  h.tick(1000 + MIN, 130, 0);   // minute 1: +30 more
  EXPECT_EQ((uint16_t)50, h.txBucket(N - 1));   // current minute = 20 + 30
  EXPECT_EQ((uint16_t)80, h.txBucket(N - 2));   // previous minute
  EXPECT_EQ(130u, h.txWindowMs());
  EXPECT_EQ((uint16_t)80, h.maxBucket(false));
}

// A gap longer than the whole window is stale history: everything is cleared.
TEST(AirtimeHistory, LongGapClearsWindow) {
  AirtimeHistory h;
  h.tick(1000, 0, 0);
  h.tick(1000, 10, 10);
  h.tick(1000 + (uint32_t)(N + 1) * MIN, 20, 20);   // slept > 1h
  EXPECT_EQ(0u, h.txWindowMs());
  EXPECT_EQ(0u, h.rxWindowMs());
}

// A counter reset (resetStats / reboot under a live sampler) makes the total go
// backwards; we skip that delta rather than banking a bogus spike.
TEST(AirtimeHistory, CounterResetSkipsDelta) {
  AirtimeHistory h;
  h.tick(1000, 1000, 0);
  h.tick(1000, 1000, 0);
  h.tick(1000, 500, 0);    // total dropped: reset detected
  EXPECT_EQ(0u, h.txWindowMs());
  h.tick(1000, 520, 0);    // resumes from the new baseline
  EXPECT_EQ(20u, h.txWindowMs());
}

// Per-bucket values are uint16; a single huge minute saturates instead of wrapping.
TEST(AirtimeHistory, SaturatesPerBucket) {
  AirtimeHistory h;
  h.tick(1000, 0, 0);
  h.tick(1000, 200000, 0);   // > 65535 in one minute
  EXPECT_EQ((uint16_t)0xFFFF, h.txBucket(N - 1));
}

// maxBucket(true) stacks TX+RX; maxBucket(false) is the max of TX or RX alone.
TEST(AirtimeHistory, MaxBucketCombinedVsSeparate) {
  AirtimeHistory h;
  h.tick(1000, 0, 0);
  h.tick(1000, 30, 40);   // one bucket: tx 30, rx 40
  EXPECT_EQ((uint16_t)70, h.maxBucket(true));
  EXPECT_EQ((uint16_t)40, h.maxBucket(false));
}

// Never returns 0, so callers can scale by it without a divide-by-zero guard.
TEST(AirtimeHistory, MaxBucketNeverZero) {
  AirtimeHistory h;
  EXPECT_EQ((uint16_t)1, h.maxBucket(true));
  EXPECT_EQ((uint16_t)1, h.maxBucket(false));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
