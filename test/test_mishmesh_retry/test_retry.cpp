// test/test_mishmesh_retry/test_retry.cpp
#include <gtest/gtest.h>
#include <mishmesh/core/RetryEngine.h>
#include <vector>

using namespace mishmesh;

namespace {

struct Action {
  enum Kind { Retransmit, Failed } kind;
  ConvoKey key;
  uint32_t senderTime;
  uint8_t  attempt;
  bool     resetPath;
};

struct RecordingActions : RetryActions {
  std::vector<Action> log;
  void retransmit(const ConvoKey& k, uint32_t st, uint8_t attempt, bool reset) override {
    log.push_back({Action::Retransmit, k, st, attempt, reset});
  }
  void markFailed(const ConvoKey& k, uint32_t st) override {
    log.push_back({Action::Failed, k, st, 0, false});
  }
};

ConvoKey dmKey(uint8_t b0) {
  ConvoKey k{}; k.type = 0; k.id[0] = b0; return k;
}

// Run one scan tick: see the given pending message, then endScan.
void scanOne(RetryEngine& eng, const ConvoKey& k, uint32_t st, uint32_t now, RetryActions& a) {
  eng.beginScan();
  eng.see(k, st, now);
  eng.endScan(now, a);
}

}  // namespace

TEST(RetryEngine, NoRetryBeforeDeadline) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(1);
  scanOne(eng, k, 100, 0, a);              // first sight at t=0
  scanOne(eng, k, 100, 7999, a);           // still inside the 8s window
  EXPECT_TRUE(a.log.empty());
  EXPECT_EQ(1, eng.trackedCount());
}

TEST(RetryEngine, RetransmitsWhenDue) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(1);
  scanOne(eng, k, 100, 0, a);
  scanOne(eng, k, 100, 8000, a);           // deadline reached
  ASSERT_EQ(1u, a.log.size());
  EXPECT_EQ(Action::Retransmit, a.log[0].kind);
  EXPECT_EQ(1, a.log[0].attempt);
  EXPECT_FALSE(a.log[0].resetPath);
}

TEST(RetryEngine, ResetsPathOnSecondRetryWhenEnabled) {
  RetryEngine eng; eng.configure(true, /*autoResetPath=*/true);
  RecordingActions a;
  ConvoKey k = dmKey(7);
  uint32_t t = 0;
  scanOne(eng, k, 50, t, a);               // first sight
  scanOne(eng, k, 50, (t += 8000), a);     // retry 1 -> no reset
  scanOne(eng, k, 50, (t += 8000), a);     // retry 2 -> reset path
  ASSERT_EQ(2u, a.log.size());
  EXPECT_EQ(1, a.log[0].attempt); EXPECT_FALSE(a.log[0].resetPath);
  EXPECT_EQ(2, a.log[1].attempt); EXPECT_TRUE(a.log[1].resetPath);
}

TEST(RetryEngine, NeverResetsPathWhenDisabled) {
  RetryEngine eng; eng.configure(true, /*autoResetPath=*/false);
  RecordingActions a;
  ConvoKey k = dmKey(7);
  uint32_t t = 0;
  scanOne(eng, k, 50, t, a);
  for (int i = 0; i < 5; i++) scanOne(eng, k, 50, (t += 8000), a);
  for (auto& act : a.log) EXPECT_FALSE(act.resetPath);
}

TEST(RetryEngine, FailsAfterFiveRetries) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(3);
  uint32_t t = 0;
  scanOne(eng, k, 9, t, a);
  // 5 retries land on the 5 due-ticks; the 6th due-tick gives up.
  for (int i = 0; i < 6; i++) scanOne(eng, k, 9, (t += 8000), a);
  ASSERT_EQ(6u, a.log.size());
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(Action::Retransmit, a.log[i].kind);
    EXPECT_EQ(i + 1, a.log[i].attempt);
  }
  EXPECT_EQ(Action::Failed, a.log[5].kind);
  EXPECT_EQ(0, eng.trackedCount());        // dropped after failing
}

TEST(RetryEngine, DeliveredMessageStopsBeingTracked) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(2);
  scanOne(eng, k, 100, 0, a);
  EXPECT_EQ(1, eng.trackedCount());
  // Next tick the message is gone from the pending set (delivered): no see().
  eng.beginScan();
  eng.endScan(16000, a);                   // well past the deadline
  EXPECT_TRUE(a.log.empty());              // not retried
  EXPECT_EQ(0, eng.trackedCount());        // reaped
}

TEST(RetryEngine, DisabledDoesNotRetry) {
  RetryEngine eng; eng.configure(/*autoRetry=*/false, false);
  RecordingActions a;
  ConvoKey k = dmKey(1);
  scanOne(eng, k, 100, 0, a);
  scanOne(eng, k, 100, 32000, a);
  EXPECT_TRUE(a.log.empty());
}

TEST(RetryEngine, TracksMultipleMessagesIndependently) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k1 = dmKey(1), k2 = dmKey(2);
  // k1 sent at t=0, k2 sent at t=4000 (staggered).
  eng.beginScan(); eng.see(k1, 10, 0); eng.endScan(0, a);
  eng.beginScan(); eng.see(k1, 10, 4000); eng.see(k2, 20, 4000); eng.endScan(4000, a);
  EXPECT_TRUE(a.log.empty());
  // t=8000: k1 due, k2 not yet.
  eng.beginScan(); eng.see(k1, 10, 8000); eng.see(k2, 20, 8000); eng.endScan(8000, a);
  ASSERT_EQ(1u, a.log.size());
  EXPECT_EQ(1u, a.log[0].key.id[0]);
  // t=12000: k2 due.
  eng.beginScan(); eng.see(k1, 10, 12000); eng.see(k2, 20, 12000); eng.endScan(12000, a);
  ASSERT_EQ(2u, a.log.size());
  EXPECT_EQ(2u, a.log[1].key.id[0]);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
