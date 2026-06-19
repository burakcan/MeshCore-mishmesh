#include <gtest/gtest.h>
#include <mishmesh/core/InputProfiler.h>

using namespace mishmesh;

// The first tick only seeds the clock; no gap is recorded from it.
TEST(InputProfiler, FirstTickSeedsOnly) {
  InputProfiler p;
  p.tick(1000);
  EXPECT_EQ(p.maxStall, 0u);
  EXPECT_EQ(p.blindGaps, 0u);
  EXPECT_EQ(p.avgLoopMs(), 0u);
}

// A steady sub-tap cadence records gaps but never a blind window.
TEST(InputProfiler, SteadyLoopNoBlindGaps) {
  InputProfiler p;
  uint32_t t = 0;
  for (int i = 0; i < 10; i++) { p.tick(t); t += 10; }
  EXPECT_EQ(p.maxStall, 10u);
  EXPECT_EQ(p.blindGaps, 0u);
  EXPECT_EQ(p.avgLoopMs(), 10u);
}

// A gap >= BLIND_GAP_MS is the starvation window that can swallow a whole tap.
TEST(InputProfiler, StallCountsAsBlindGap) {
  InputProfiler p;
  p.tick(0);
  p.tick(10);                 // 10ms gap: fine
  p.tick(10 + InputProfiler::BLIND_GAP_MS);       // exactly the threshold: blind
  p.tick(10 + InputProfiler::BLIND_GAP_MS + 200); // 200ms stall: blind + new max
  EXPECT_EQ(p.blindGaps, 2u);
  EXPECT_EQ(p.maxStall, 200u);
}

TEST(InputProfiler, RenderTracksMaxAndLast) {
  InputProfiler p;
  p.recordRender(4);
  p.recordRender(30);
  p.recordRender(12);
  EXPECT_EQ(p.maxRender, 30u);
  EXPECT_EQ(p.lastRender, 12u);
  EXPECT_EQ(p.renders, 3u);
}

// polled = presses that reached software; dispatched = presses acted on.
// The gap between them is what software gating (bounce/debounce) ate.
TEST(InputProfiler, InputCountersAreIndependent) {
  InputProfiler p;
  p.recordPolled(); p.recordPolled(); p.recordPolled();
  p.recordDispatched(); p.recordDispatched();
  EXPECT_EQ(p.polled, 3u);
  EXPECT_EQ(p.dispatched, 2u);
}

TEST(InputProfiler, ResetClearsEverything) {
  InputProfiler p;
  p.tick(0); p.tick(100);
  p.recordRender(9); p.recordPolled(); p.recordDispatched();
  p.reset();
  EXPECT_EQ(p.maxStall, 0u);
  EXPECT_EQ(p.blindGaps, 0u);
  EXPECT_EQ(p.maxRender, 0u);
  EXPECT_EQ(p.polled, 0u);
  EXPECT_EQ(p.dispatched, 0u);
  // After reset the next tick re-seeds (no phantom gap from the pre-reset time).
  p.tick(1000);
  EXPECT_EQ(p.maxStall, 0u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
