// test/test_mishmesh_retry/test_retry.cpp
#include <gtest/gtest.h>
#include <mishmesh/core/RetryEngine.h>
#include <mishmesh/core/MessageStore.h>
#include "FakeMsgLogBackend.h"
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

// Run one scan tick: the given message is still pending, then endScan.
void scanOne(RetryEngine& eng, const ConvoKey& k, uint32_t st, uint32_t now, RetryActions& a) {
  eng.beginScan();
  eng.see(k, st);
  eng.endScan(now, a);
}

}  // namespace

TEST(RetryEngine, NoRetryBeforeDeadline) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(1);
  eng.track(k, 100, 0);                    // sent at t=0
  scanOne(eng, k, 100, 0, a);
  scanOne(eng, k, 100, 7999, a);           // still inside the 8s window
  EXPECT_TRUE(a.log.empty());
  EXPECT_EQ(1, eng.trackedCount());
}

TEST(RetryEngine, RetransmitsWhenDue) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(1);
  eng.track(k, 100, 0);
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
  eng.track(k, 50, t);
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
  eng.track(k, 50, t);
  for (int i = 0; i < 5; i++) scanOne(eng, k, 50, (t += 8000), a);
  for (auto& act : a.log) EXPECT_FALSE(act.resetPath);
}

TEST(RetryEngine, FailsAfterFiveRetries) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(3);
  uint32_t t = 0;
  eng.track(k, 9, t);
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
  eng.track(k, 100, 0);
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
  eng.track(k, 100, 0);
  scanOne(eng, k, 100, 32000, a);
  EXPECT_TRUE(a.log.empty());
}

TEST(RetryEngine, TracksMultipleMessagesIndependently) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k1 = dmKey(1), k2 = dmKey(2);
  // k1 sent at t=0, k2 sent at t=4000 (staggered).
  eng.track(k1, 10, 0);
  eng.beginScan(); eng.see(k1, 10); eng.endScan(0, a);
  eng.track(k2, 20, 4000);
  eng.beginScan(); eng.see(k1, 10); eng.see(k2, 20); eng.endScan(4000, a);
  EXPECT_TRUE(a.log.empty());
  // t=8000: k1 due, k2 not yet.
  eng.beginScan(); eng.see(k1, 10); eng.see(k2, 20); eng.endScan(8000, a);
  ASSERT_EQ(1u, a.log.size());
  EXPECT_EQ(1u, a.log[0].key.id[0]);
  // t=12000: k2 due.
  eng.beginScan(); eng.see(k1, 10); eng.see(k2, 20); eng.endScan(12000, a);
  ASSERT_EQ(2u, a.log.size());
  EXPECT_EQ(2u, a.log[1].key.id[0]);
}

// Only the send path may create entries: an untracked message reported by a scan
// is ignored, so nothing the store still holds can re-enter the retry set.
TEST(RetryEngine, SeeDoesNotCreateEntries) {
  RetryEngine eng; eng.configure(true, false);
  RecordingActions a;
  ConvoKey k = dmKey(4);
  uint32_t t = 0;
  for (int i = 0; i < 10; i++) scanOne(eng, k, 77, (t += 8000), a);
  EXPECT_EQ(0, eng.trackedCount());
  EXPECT_TRUE(a.log.empty());
}

TEST(RetryEngine, TrackIgnoresChannelsDuplicatesAndOverflow) {
  RetryEngine eng; eng.configure(true, false);
  ConvoKey ch{}; ch.type = 1; ch.id[0] = 3;
  eng.track(ch, 1, 0);
  EXPECT_EQ(0, eng.trackedCount());                  // channel messages get no ACK retry
  eng.track(dmKey(1), 1, 0);
  eng.track(dmKey(1), 1, 0);
  EXPECT_EQ(1, eng.trackedCount());                  // same message twice = one entry
  for (int i = 0; i < RetryEngine::MAX_PENDING + 4; i++) eng.track(dmKey(9), 100 + i, 0);
  EXPECT_EQ((int)RetryEngine::MAX_PENDING, eng.trackedCount());
}

TEST(RetryEngine, SnapshotListsTrackedMessages) {
  RetryEngine eng; eng.configure(true, false);
  eng.track(dmKey(1), 10, 0);
  eng.track(dmKey(2), 20, 0);
  ConvoKey keys[RetryEngine::MAX_PENDING]; uint32_t times[RetryEngine::MAX_PENDING];
  ASSERT_EQ(2, eng.snapshot(keys, times));
  EXPECT_EQ(1u, keys[0].id[0]); EXPECT_EQ(10u, times[0]);
  EXPECT_EQ(2u, keys[1].id[0]); EXPECT_EQ(20u, times[1]);
}

// Regression for the infinite retry flood (issue #18): with more undelivered DMs
// in the store than the engine can hold, and chat ranking churning as replies
// arrive, retries must still terminate. The old scan collected the store's first
// MAX_PENDING pending records in recency-rank order, so the set rotated under
// churn and every rotation reset the attempt count.
TEST(RetryFlood, TerminatesWithBacklogAndChurn) {
  FakeMsgLogBackend backend;
  MessageStore store; store.begin(&backend);
  RetryEngine eng; eng.configure(true, false);

  struct Glue : RetryActions {
    MessageStore* store;
    int tx = 0, failed = 0;
    void retransmit(const ConvoKey&, uint32_t, uint8_t, bool) override { tx++; }
    void markFailed(const ConvoKey& k, uint32_t st) override { failed++; store->markFailed(k, st); }
  } glue; glue.store = &store;

  const int CONVOS = 3, PER_CONVO = 4;              // 12 undelivered DMs: > MAX_PENDING
  uint32_t st = 1000;
  for (int c = 0; c < CONVOS; c++)
    for (int m = 0; m < PER_CONVO; m++) {
      ConvoKey k{}; k.type = 0; k.id[0] = (uint8_t)(0xA0 + c);
      store.appendOutboundDM(k, "Clock", 5, st, st, 0, 0);
      st++;
    }
  // Only two of them are this session's sends; the rest are backlog.
  ConvoKey live0{}; live0.type = 0; live0.id[0] = 0xA0;
  ConvoKey live1{}; live1.type = 0; live1.id[0] = 0xA2;
  eng.track(live0, 1000, 0);
  eng.track(live1, 1011, 0);

  uint32_t now = 0;
  for (int tick = 0; tick < 200; tick++) {          // 200 * 6s = 20 minutes
    now += 6000;
    // a reply lands in a different chat each tick -> recency ranking churns
    ConvoKey rk{}; rk.type = 0; rk.id[0] = (uint8_t)(0xA0 + (tick % CONVOS));
    store.appendInbound(rk, "r", 1, now / 1000, now / 1000, 0, nullptr, 0);

    ConvoKey pk[RetryEngine::MAX_PENDING]; uint32_t pt[RetryEngine::MAX_PENDING];
    bool pending[RetryEngine::MAX_PENDING];
    int pn = eng.snapshot(pk, pt);
    store.checkPendingDMs(pk, pt, pending, pn);
    eng.beginScan();
    for (int i = 0; i < pn; i++) if (pending[i]) eng.see(pk[i], pt[i]);
    eng.endScan(now, glue);
  }

  EXPECT_EQ(2 * RetryEngine::MAX_RETRIES, glue.tx);  // 5 retries each, then done
  EXPECT_EQ(2, glue.failed);
  EXPECT_EQ(0, eng.trackedCount());
  EXPECT_EQ(10, store.pendingDMCount());             // backlog untouched, never retried
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
