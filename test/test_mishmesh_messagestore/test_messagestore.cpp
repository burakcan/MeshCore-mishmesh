// test/test_mishmesh_messagestore/test_messagestore.cpp
#include <gtest/gtest.h>
#include <mishmesh/core/MessageStore.h>
#include <mishmesh/core/RetryEngine.h>
#include "FakeMsgLogBackend.h"
#include <cstring>
using namespace mishmesh;

static ConvoKey dm(const char* p) { return directKey((const uint8_t*)p); }

struct MSFix : ::testing::Test {
  mishmesh::FakeMsgLogBackend backend;
  mishmesh::MessageStore s;
  void SetUp() override { s.begin(&backend); }
};

TEST_F(MSFix, StartsEmpty) {
  EXPECT_EQ(0, s.convoCount());
  EXPECT_EQ(0u, s.totalUnread());
}

TEST_F(MSFix, InboundCreatesConvoAndMessage) {
  uint8_t path[2] = {0x5B, 0x63};
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, /*snrx4*/-32, path, 2);
  EXPECT_EQ(1, s.convoCount());
  EXPECT_EQ(1, s.messageCount(dm("ALICE!")));
  MsgRecord r;
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(KIND_INBOUND, r.kind);
  EXPECT_EQ(2, r.textLen);
  EXPECT_EQ(0, memcmp(r.text, "hi", 2));
  EXPECT_EQ(2000u, r.localTime);
  EXPECT_EQ(-32, r.snrx4);
  EXPECT_EQ(2, r.pathLen);
  EXPECT_EQ(0x63, r.path[1]);
}

TEST_F(MSFix, InboundPreservesMultibytePathHashes) {
  uint8_t path[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, -32, path, 0x42);
  MsgRecord r;
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(0x42, r.pathLen);
  EXPECT_EQ(2, r.hops);
  EXPECT_EQ(0, memcmp(path, r.path, 4));
}

TEST_F(MSFix, RecencySortedNewestFirst) {
  s.appendInbound(dm("ALICE!"), "a", 1, 100, 100, 0, nullptr, 0);
  s.appendInbound(dm("BOBBBB"), "b", 1, 200, 200, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "c", 1, 300, 300, 0, nullptr, 0);  // Alice now newest
  ASSERT_EQ(2, s.convoCount());
  ConvoSummary c0, c1;
  ASSERT_TRUE(s.getConvo(0, c0));
  ASSERT_TRUE(s.getConvo(1, c1));
  EXPECT_TRUE(c0.key.equals(dm("ALICE!")));   // most recent first
  EXPECT_TRUE(c1.key.equals(dm("BOBBBB")));
  EXPECT_EQ(2, s.messageCount(dm("ALICE!")));
}

TEST_F(MSFix, UnreadIncrementsWhenInactive) {
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "b", 1, 2, 2, 0, nullptr, 0);
  EXPECT_EQ(2u, s.totalUnread());
}

TEST_F(MSFix, ActiveConvoSuppressesAndClears) {
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);  // unread = 1
  s.setActiveConvo(dm("ALICE!"));                               // clears Alice
  EXPECT_EQ(0u, s.totalUnread());
  s.appendInbound(dm("ALICE!"), "b", 1, 2, 2, 0, nullptr, 0);  // active -> no badge
  EXPECT_EQ(0u, s.totalUnread());
  s.appendInbound(dm("BOBBBB"), "c", 1, 3, 3, 0, nullptr, 0);  // Bob inactive -> badge
  EXPECT_EQ(1u, s.totalUnread());
  s.clearActiveConvo();
  s.appendInbound(dm("ALICE!"), "d", 1, 4, 4, 0, nullptr, 0);  // now badges again
  EXPECT_EQ(2u, s.totalUnread());
}

TEST_F(MSFix, LastInboundTracksSubjectChat) {
  ConvoKey k;
  EXPECT_FALSE(s.lastInbound(k));                 // nothing captured yet
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  EXPECT_TRUE(s.lastInbound(k));
  EXPECT_TRUE(k.equals(dm("ALICE!")));
  s.appendInbound(dm("BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  EXPECT_TRUE(s.lastInbound(k));
  EXPECT_TRUE(k.equals(dm("BOBBBB")));            // follows the newest arrival
}

TEST_F(MSFix, ActiveConvoReportsOpenChat) {
  ConvoKey k;
  EXPECT_FALSE(s.activeConvo(k));                 // no chat open
  s.setActiveConvo(dm("ALICE!"));
  EXPECT_TRUE(s.activeConvo(k));
  EXPECT_TRUE(k.equals(dm("ALICE!")));
  s.clearActiveConvo();
  EXPECT_FALSE(s.activeConvo(k));
}

TEST_F(MSFix, PerChatCapDropsOwnOldest) {
  // Batch rotation drops 16 records at once when count hits the cap.
  // With PER_CHAT_CAP+5 messages, one rotation fires (at msg 101), dropping 16
  // records, leaving count in the range [PER_CHAT_CAP-16, PER_CHAT_CAP].
  for (int i = 0; i < PER_CHAT_CAP + 5; i++)
    s.appendInbound(dm("ALICE!"), "x", 1, i + 1, i + 1, 0, nullptr, 0);
  int count = s.messageCount(dm("ALICE!"));
  EXPECT_LE(count, PER_CHAT_CAP);   // cap enforced
  EXPECT_GT(count, 0);
  MsgRecord r;
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_GT(r.senderTime, 1u);   // oldest messages were dropped
}

TEST_F(MSFix, CompactionReclaimsTombstones) {
  // Batch rotation: count stays bounded at or below PER_CHAT_CAP.
  for (int i = 0; i < PER_CHAT_CAP + 50; i++)
    s.appendInbound(dm("ALICE!"), "x", 1, i + 1, i + 1, 0, nullptr, 0);
  EXPECT_LE(s.messageCount(dm("ALICE!")), PER_CHAT_CAP);   // stays bounded
}

TEST_F(MSFix, BackstopEvictsLargestChatNotQuietDM) {
  // Quiet DM with a unique marker, added first (oldest, smallest convo).
  s.appendInbound(dm("ALICE!"), "KEEP", 4, 1, 1, 0, nullptr, 0);

  // Fill with 120-byte messages spread across several channels.
  // With PER_CHAT_CAP=100, 50 per channel is well under cap - all stored.
  char big[120]; memset(big, 'x', sizeof(big));
  const int K = 4;
  uint32_t t = 100;
  for (int i = 0; i < 200; i++, t++)
    s.appendInbound(channelKey((uint8_t)(1 + (i % K))), big, sizeof(big), t, t, 0, nullptr, 0);

  // Final, most-recent message carries a marker so we can confirm it was stored.
  char newest[120]; memset(newest, 'y', sizeof(newest)); memcpy(newest, "NEWEST", 6);
  s.appendInbound(channelKey(1), newest, sizeof(newest), t, t, 0, nullptr, 0);

  // (a) The quiet DM survives.
  ASSERT_EQ(1, s.messageCount(dm("ALICE!")));
  MsgRecord dr; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, dr));
  EXPECT_EQ(0, memcmp(dr.text, "KEEP", 4));

  // (b) The most-recent channel message was stored.
  int n = s.messageCount(channelKey(1));
  ASSERT_GT(n, 0);
  MsgRecord cr; ASSERT_TRUE(s.getMessage(channelKey(1), n - 1, cr));
  EXPECT_EQ(0, memcmp(cr.text, "NEWEST", 6));
}

TEST_F(MSFix, BackstopEvictsLRUChatUnderCapacity) {
  // Small backend so the physical space check fires well before the budget.
  // Each inbound record with 3-byte text: REC_HDR(17) + 3 + 2 = 22 bytes.
  backend.capacity = 4096;

  // AAAAA! filled first (older -> LRU). 110 messages triggers 10 rotations;
  // after rotation stabilises, AAAAA!'s file holds 100 live records = 2200 bytes.
  for (int i = 0; i < 110; i++)
    s.appendInbound(dm("AAAAA!"), "old", 3, i+1, i+1, 0, nullptr, 0);

  // BBBBB! added later (newer -> MRU). 2200 bytes used by AAAAA!, 1896 free.
  // 1896/22 ~ 86 messages before capacity is exhausted. ensureSpace then evicts
  // AAAAA! (LRU) to make room; subsequent BBBBB! messages succeed.
  for (int i = 0; i < 100; i++)
    s.appendInbound(dm("BBBBB!"), "new", 3, 200+i, 200+i, 0, nullptr, 0);

  // AAAAA! slot evicted from the index under capacity pressure.
  EXPECT_EQ(0, s.messageCount(dm("AAAAA!")));
  // BBBBB! has messages retained.
  EXPECT_GT(s.messageCount(dm("BBBBB!")), 0);
}

TEST_F(MSFix, ConvoOverflowEvictsLeastRecent) {
  auto keyOf = [](int i) {
    char nm[7] = "C00000";
    nm[5] = (char)('A' + (i % 20)); nm[4] = (char)('0' + i / 20);
    return directKey((const uint8_t*)nm);
  };
  for (int i = 0; i < MAX_CONVOS + 3; i++)
    s.appendInbound(keyOf(i), "m", 1, i + 1, i + 1, 0, nullptr, 0);

  EXPECT_EQ(MAX_CONVOS, s.convoCount());
  EXPECT_EQ(0, s.messageCount(keyOf(0)));                // oldest evicted
  EXPECT_EQ(0, s.messageCount(keyOf(1)));
  EXPECT_EQ(0, s.messageCount(keyOf(2)));
  EXPECT_EQ(1, s.messageCount(keyOf(MAX_CONVOS)));       // newest survive
  EXPECT_EQ(1, s.messageCount(keyOf(MAX_CONVOS + 2)));
}

TEST_F(MSFix, OutboundDMStartsPendingThenDelivered) {
  s.appendOutboundDM(dm("ALICE!"), "yo", 2, 500, 500, /*ack*/0xDEADBEEF, /*sentMs*/1000);
  MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(KIND_OUT_DM, r.kind);
  EXPECT_EQ(ST_PENDING, r.status);
  s.markDelivered(0xDEADBEEF, /*trip*/1234);
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(ST_DELIVERED, r.status);
  EXPECT_EQ(1234, r.tripTimeMs);
}

TEST_F(MSFix, PendingDMEnumerationSkipsNonPending) {
  s.appendOutboundDM(dm("ALICE!"), "a", 1, 100, 100, 0x1111, 0);
  s.appendOutboundDM(dm("BOBBBB"), "b", 1, 200, 200, 0x2222, 0);
  s.appendInbound(dm("CAROL!"), "in", 2, 300, 300, 0, nullptr, 0);   // inbound: not counted
  s.appendOutboundChannel(channelKey(4), "c", 1, 400, 400);          // channel: not counted
  EXPECT_EQ(2, s.pendingDMCount());
  s.markDelivered(0x1111, 0);                                        // ALICE delivered -> drops out
  EXPECT_EQ(1, s.pendingDMCount());
  ConvoKey k; uint32_t st = 0;
  ASSERT_TRUE(s.getPendingDM(0, k, st));
  EXPECT_TRUE(k.equals(dm("BOBBBB")));
  EXPECT_EQ(200u, st);
  EXPECT_FALSE(s.getPendingDM(1, k, st));                            // only one left
}

TEST_F(MSFix, CheckPendingDMsReportsPerEntry) {
  s.appendOutboundDM(dm("ALICE!"), "a", 1, 100, 100, 0x1111, 0);
  s.appendOutboundDM(dm("ALICE!"), "b", 1, 101, 101, 0x1112, 0);
  s.appendOutboundDM(dm("BOBBBB"), "c", 1, 200, 200, 0x2222, 0);
  ConvoKey keys[4] = { dm("ALICE!"), dm("ALICE!"), dm("BOBBBB"), dm("CAROL!") };
  uint32_t times[4] = { 100, 101, 200, 300 };
  bool pending[4];
  s.checkPendingDMs(keys, times, pending, 4);
  EXPECT_TRUE(pending[0]);
  EXPECT_TRUE(pending[1]);
  EXPECT_TRUE(pending[2]);
  EXPECT_FALSE(pending[3]);                                          // no such message
  s.markDelivered(0x1111, 0);
  s.markFailed(dm("BOBBBB"), 200);
  s.checkPendingDMs(keys, times, pending, 4);
  EXPECT_FALSE(pending[0]);
  EXPECT_TRUE(pending[1]);
  EXPECT_FALSE(pending[2]);
}

// The retry engine reads this as "still in flight", so an entry it asks about
// must never be dropped for being one of many - there is no cap to fall off.
TEST_F(MSFix, CheckPendingDMsHasNoCap) {
  ConvoKey keys[RetryEngine::MAX_PENDING]; uint32_t times[RetryEngine::MAX_PENDING];
  bool pending[RetryEngine::MAX_PENDING];
  for (int i = 0; i < RetryEngine::MAX_PENDING; i++) {
    s.appendOutboundDM(dm("ALICE!"), "x", 1, 500 + i, 500 + i, 0x3000 + i, 0);
    keys[i] = dm("ALICE!"); times[i] = 500 + i;
  }
  s.appendOutboundDM(dm("BOBBBB"), "y", 1, 900, 900, 0x4000, 0);     // extra pending elsewhere
  s.checkPendingDMs(keys, times, pending, RetryEngine::MAX_PENDING);
  for (int i = 0; i < RetryEngine::MAX_PENDING; i++) EXPECT_TRUE(pending[i]);
}

TEST_F(MSFix, FailStalePendingDMsSettlesPreviousSession) {
  s.appendOutboundDM(dm("ALICE!"), "a", 1, 100, 100, 0x1111, 0);
  s.appendOutboundDM(dm("ALICE!"), "b", 1, 101, 101, 0x1112, 0);
  s.appendOutboundDM(dm("BOBBBB"), "c", 1, 200, 200, 0x2222, 0);
  s.markDelivered(0x1112, 42);
  s.appendInbound(dm("CAROL!"), "in", 2, 300, 300, 0, nullptr, 0);
  s.appendOutboundChannel(channelKey(4), "d", 1, 400, 400);          // channel: untouched
  EXPECT_EQ(2, s.failStalePendingDMs());
  EXPECT_EQ(0, s.pendingDMCount());
  MsgRecord r;
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r)); EXPECT_EQ(ST_FAILED, r.status);
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 1, r)); EXPECT_EQ(ST_DELIVERED, r.status);
  ASSERT_TRUE(s.getMessage(dm("BOBBBB"), 0, r)); EXPECT_EQ(ST_FAILED, r.status);
  EXPECT_EQ(0, s.failStalePendingDMs());                             // nothing left to settle
}

TEST_F(MSFix, GetDMTextReturnsBody) {
  s.appendOutboundDM(dm("ALICE!"), "hello world", 11, 500, 500, 0xABCD, 0);
  char buf[32];
  int n = s.getDMText(dm("ALICE!"), 500, buf, sizeof(buf));
  EXPECT_EQ(11, n);
  EXPECT_STREQ("hello world", buf);
  EXPECT_EQ(0, s.getDMText(dm("ALICE!"), 999, buf, sizeof(buf)));    // wrong senderTime
}

TEST_F(MSFix, MarkFailedSetsStatus) {
  s.appendOutboundDM(dm("ALICE!"), "x", 1, 500, 500, 0xACE, 0);
  s.markFailed(dm("ALICE!"), 500);
  MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(ST_FAILED, r.status);
  EXPECT_EQ(0, s.pendingDMCount());
}

TEST_F(MSFix, MarkFailedLeavesDeliveredAlone) {
  s.appendOutboundDM(dm("ALICE!"), "x", 1, 500, 500, 0xACE, 0);
  s.markDelivered(0xACE, 7);
  s.markFailed(dm("ALICE!"), 500);                                   // already delivered -> no-op
  MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(ST_DELIVERED, r.status);
}

TEST_F(MSFix, UpdateExpectedAckRepointsDelivery) {
  s.appendOutboundDM(dm("ALICE!"), "x", 1, 500, 500, /*ack*/0x1111, 0);
  s.updateExpectedAck(dm("ALICE!"), 500, /*newAck*/0x2222);
  s.markDelivered(0x1111, 9);                                        // stale ack: no match now
  MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(ST_PENDING, r.status);
  s.markDelivered(0x2222, 9);                                        // new ack matches
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(ST_DELIVERED, r.status);
}

TEST_F(MSFix, ChannelRepeatsAccumulate) {
  s.appendOutboundChannel(channelKey(2), "hello", 5, 700, 700);
  uint8_t p1[2] = {0x33, 0x44}, p2[1] = {0xDA};
  s.addRepeat(channelKey(2), 700, /*snrx4*/29, p1, 2);
  s.addRepeat(channelKey(2), 700, /*snrx4*/-2, p2, 1);
  MsgRecord r; ASSERT_TRUE(s.getMessage(channelKey(2), 0, r));
  EXPECT_EQ(KIND_OUT_CHAN, r.kind);
  EXPECT_EQ(ST_SENT, r.status);
  EXPECT_EQ(2, r.heardCount);
  EXPECT_EQ(2, s.repeatCount(channelKey(2), 0));
  RepeatRec rr; ASSERT_TRUE(s.getRepeat(channelKey(2), 0, 0, rr));
  EXPECT_EQ(29, rr.snrx4);
  EXPECT_EQ(0x44, rr.path[rr.pathLen - 1]);   // last hop = rebroadcasting repeater
}

TEST_F(MSFix, RepeatsCapAtMax) {
  s.appendOutboundChannel(channelKey(2), "x", 1, 700, 700);
  uint8_t p[1] = {0x10};
  for (int i = 0; i < MAX_REPEATS + 4; i++) s.addRepeat(channelKey(2), 700, (int8_t)i, p, 1);
  EXPECT_EQ(MAX_REPEATS, s.repeatCount(channelKey(2), 0));
  MsgRecord r; ASSERT_TRUE(s.getMessage(channelKey(2), 0, r));
  EXPECT_EQ(MAX_REPEATS + 4, r.heardCount);   // count keeps climbing even past stored detail
}

TEST_F(MSFix, RepeatPathSurvivesTrackingEviction) {
  for (int c = 1; c <= MAX_TRACKED + 1; c++) {
    s.appendOutboundChannel(channelKey((uint8_t)c), "m", 1, /*senderTime*/700 + c, 700 + c);
    uint8_t path[2] = {(uint8_t)(0xA0 + c), (uint8_t)(0xB0 + c)};
    s.addRepeat(channelKey((uint8_t)c), 700 + c, /*snrx4*/(int8_t)c, path, 2);
  }
  // channel 1 was evicted; channel 2 is the oldest survivor.
  RepeatRec rr; ASSERT_TRUE(s.getRepeat(channelKey(2), 0, 0, rr));
  EXPECT_EQ(2, rr.pathLen);
  EXPECT_EQ(0xA2, rr.path[0]);
  EXPECT_EQ(0xB2, rr.path[1]);
  // channel 1 fell out of tracking entirely -> summary-only.
  EXPECT_EQ(0, s.repeatCount(channelKey(1), 0));
}

TEST_F(MSFix, DeleteMessageRemovesOne) {
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "b", 1, 2, 2, 0, nullptr, 0);
  s.deleteMessage(dm("ALICE!"), 0);
  EXPECT_EQ(1, s.messageCount(dm("ALICE!")));
  MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(0, memcmp(r.text, "b", 1));
}

TEST_F(MSFix, ClearConvoKeepsEmptyChat) {
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  s.clearConvo(dm("ALICE!"));
  EXPECT_EQ(2, s.convoCount());                 // chat stays in the list
  EXPECT_EQ(0, s.messageCount(dm("ALICE!")));   // ...but emptied
  EXPECT_EQ(1, s.messageCount(dm("BOBBBB")));   // other chat untouched
}

TEST_F(MSFix, DeletingLastMessageKeepsChat) {
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.deleteMessage(dm("ALICE!"), 0);
  EXPECT_EQ(1, s.convoCount());                 // chat persists even with no messages
  EXPECT_EQ(0, s.messageCount(dm("ALICE!")));
}

TEST_F(MSFix, DeleteConvoRemovesChatEntirely) {
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  s.deleteConvo(dm("ALICE!"));
  EXPECT_EQ(1, s.convoCount());                 // ALICE gone from the list
  EXPECT_EQ(0, s.messageCount(dm("ALICE!")));
  EXPECT_EQ(1, s.messageCount(dm("BOBBBB")));   // other chat untouched
}

TEST_F(MSFix, MarkUnreadSetsUnread) {
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.setActiveConvo(dm("ALICE!"));               // clears unread
  EXPECT_EQ(0u, s.totalUnread());
  s.clearActiveConvo();
  s.markUnread(dm("ALICE!"));
  EXPECT_GT(s.totalUnread(), 0u);
}

// serialize/deserialize (MSFL blob) tests removed: persistence is now
// backend-based (per-chat log files + saveIndex()); the old whole-store
// snapshot path is no longer exercised.

TEST_F(MSFix, NotifiableCounterIndependentOfUnread) {
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "yo", 2, 1001, 2001, 0, nullptr, 0);
  EXPECT_EQ(2u, s.totalUnread());
  EXPECT_EQ(0u, s.totalNotifyUnread());   // nothing marked notifiable yet
  s.markNotifiable(dm("ALICE!"));
  EXPECT_EQ(1u, s.totalNotifyUnread());
  EXPECT_EQ(2u, s.totalUnread());          // unread untouched by markNotifiable
}

TEST_F(MSFix, OpeningChatClearsBothCounters) {
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.markNotifiable(dm("ALICE!"));
  s.setActiveConvo(dm("ALICE!"));          // user opens the chat
  EXPECT_EQ(0u, s.totalUnread());
  EXPECT_EQ(0u, s.totalNotifyUnread());
}

TEST_F(MSFix, NotifiableSkippedForActiveConvo) {
  s.setActiveConvo(dm("ALICE!"));
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.markNotifiable(dm("ALICE!"));          // chat is open -> no notifiable bump
  EXPECT_EQ(0u, s.totalNotifyUnread());
}

// SerializeRoundTripsNotifyUnread removed: MSFL blob path replaced by saveIndex.

TEST_F(MSFix, ReadsBeyondWindowByPaging) {
  char body[80];
  for (int i = 0; i < 90; i++) {
    snprintf(body, sizeof(body), "message-number-%02d-padded-padded-padded-pad", i);
    s.appendInbound(dm("ALICE!"), body, (uint16_t)strlen(body), 1000+i, 2000+i, 0, nullptr, 0);
  }
  s.setActiveConvo(dm("ALICE!"));
  EXPECT_EQ(90, s.messageCount(dm("ALICE!")));
  MsgRecord r0, r89;
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r0));    // oldest - paged from flash
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 89, r89));  // newest - in window
  EXPECT_EQ(0, memcmp(r0.text, "message-number-00", 17));
  EXPECT_EQ(0, memcmp(r89.text, "message-number-89", 17));
}

// Regression: ensureSpace must not remove the index slot when the append target IS the LRU.
// Bug: evictLRU(v) removed the slot first; v.equals(key) guard fired too late -> slot orphaned.
TEST_F(MSFix, EnsureSpaceDoesNotOrphanOnlyChatWhenLRU) {
  // Inbound record with 3-byte text: REC_HDR(17) + 3 + 2(snrx4+pathLen) = 22 bytes.
  // Capacity of 40 fits one record; second append triggers ensureSpace on the same chat.
  backend.capacity = 40;

  s.appendInbound(dm("SOLO!!"), "aaa", 3, 1, 1, 0, nullptr, 0);
  ASSERT_EQ(1, s.convoCount());
  ASSERT_EQ(1, s.messageCount(dm("SOLO!!")));

  // SOLO!! is the only chat (and therefore the LRU). ensureSpace must return false
  // (drop message) without removing the index slot - leaving count consistent with flash.
  s.appendInbound(dm("SOLO!!"), "bbb", 3, 2, 2, 0, nullptr, 0);

  EXPECT_EQ(1, s.convoCount());                  // slot must survive
  EXPECT_EQ(1, s.messageCount(dm("SOLO!!")));    // count must match the one message on flash
}

TEST_F(MSFix, RebuildIndexRestoresPreview) {
  s.appendInbound(dm("ALICE!"), "first msg", 9, 1000, 2000, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "last msg", 8, 1001, 2001, 0, nullptr, 0);
  backend.index.clear();  // simulate index loss (power loss, etc.)
  MessageStore s2;
  s2.begin(&backend);     // must rebuild preview from the on-flash log
  ASSERT_EQ(1, s2.convoCount());
  ConvoSummary cs;
  ASSERT_TRUE(s2.getConvo(0, cs));
  EXPECT_GT(cs.preview[0], 0);                       // non-empty
  EXPECT_EQ(0, memcmp(cs.preview, "last msg", 8));   // matches last appended body
}

// Reproduces the reported bug: unread counts must survive a normal reboot, where the
// index blob was flushed to flash (saveIndex) and is intact on the next begin().
TEST_F(MSFix, UnreadSurvivesRebootViaIndexBlob) {
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);   // unread = 1
  s.appendInbound(dm("BOBBBB"), "yo", 2, 1001, 2001, 0, nullptr, 0);   // unread = 1
  s.markNotifiable(dm("ALICE!"));                                       // notifyUnread = 1
  ASSERT_EQ(2u, s.totalUnread());
  ASSERT_EQ(1u, s.totalNotifyUnread());
  s.saveIndex();               // debounced flush would have done this after the messages arrived

  MessageStore s2;
  s2.begin(&backend);          // reboot: index blob intact -> deserialize path (NOT rebuild)
  EXPECT_EQ(2u, s2.totalUnread());
  EXPECT_EQ(1u, s2.totalNotifyUnread());
}

TEST_F(MSFix, RebuildIndexFromLogsWhenIndexMissing) {
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.saveIndex();          // save index so it exists, then simulate it being lost
  backend.index.clear();  // index blob gone (power loss after write, before sync, etc.)
  MessageStore s2;
  s2.begin(&backend);     // must rebuild from the on-flash log file
  EXPECT_EQ(1, s2.convoCount());
  EXPECT_EQ(1, s2.messageCount(dm("ALICE!")));
}

TEST_F(MSFix, TornTrailingRecordIsIgnored) {
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  // Append a partial record header to simulate a crash mid-write
  char name[15]; codec::keyToName(dm("ALICE!"), name);
  uint8_t junk[3] = {0xAB, 0xCD, 0xEF};
  backend.files[name].insert(backend.files[name].end(), junk, junk + 3);
  backend.index.clear();
  MessageStore s2;
  s2.begin(&backend);     // rebuild; torn tail must be ignored, not counted
  EXPECT_EQ(1, s2.convoCount());
  EXPECT_EQ(1, s2.messageCount(dm("ALICE!")));
}

// Regression: large file (> WINDOW_BYTES) torn tail must be physically healed via truncate()
// so a subsequent append doesn't land after garbage bytes.
TEST_F(MSFix, LargeFileTornTailIsPhysicallyHealed) {
  // Inbound with 60-byte text: REC_HDR(17) + 60 + 2(snrx4+pathLen) = 79 bytes each.
  // 90 records = 7110 bytes > WINDOW_BYTES(5120), so the old rewrite-path would skip healing.
  char body[60];
  const int COUNT = 90;
  for (int i = 0; i < COUNT; i++) {
    snprintf(body, sizeof(body), "large-file-msg-%02d-padding-padding-padding-pad", i);
    s.appendInbound(dm("LARGE!"), body, (uint16_t)strlen(body), 1000 + i, 2000 + i, 0, nullptr, 0);
  }
  ASSERT_EQ(COUNT, s.messageCount(dm("LARGE!")));

  // Record valid file size before injecting the torn tail.
  char name[15]; codec::keyToName(dm("LARGE!"), name);
  uint32_t validSize = backend.size(name);
  ASSERT_GT(validSize, 5120u);  // confirm we're in the large-file path (> WINDOW_BYTES)

  // Inject a partial record (5 bytes of 0xFF) to simulate a crash mid-write.
  uint8_t torn[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  backend.files[name].insert(backend.files[name].end(), torn, torn + sizeof(torn));
  ASSERT_EQ(validSize + sizeof(torn), backend.size(name));

  // Wipe the index to force rebuildIndex on next begin().
  backend.index.clear();
  MessageStore s2;
  s2.begin(&backend);  // must call truncate() to physically remove the torn tail

  // Torn tail physically removed - file is back to validSize.
  EXPECT_EQ(validSize, backend.size(name));
  // All valid records accounted for.
  EXPECT_EQ(COUNT, s2.messageCount(dm("LARGE!")));

  // Append a new message and confirm it's readable (proves append-after-heal is not corrupted).
  s2.appendInbound(dm("LARGE!"), "new-after-heal", 14, 9999, 9999, 0, nullptr, 0);
  EXPECT_EQ(COUNT + 1, s2.messageCount(dm("LARGE!")));
  MsgRecord r;
  ASSERT_TRUE(s2.getMessage(dm("LARGE!"), COUNT, r));
  EXPECT_EQ(14, r.textLen);
  EXPECT_EQ(0, memcmp(r.text, "new-after-heal", 14));
}

// forEachMessage must yield exactly the same records, in the same order, as
// getMessage(0..n-1) - including across the window/paging boundary (this is the
// O(n) sequential path the thread view relies on for full-thread relayout).
TEST_F(MSFix, ForEachMessageMatchesGetMessage) {
  char body[80];
  for (int i = 0; i < 60; i++) {   // enough to exceed the RAM window -> exercises paging
    snprintf(body, sizeof(body), "seq-%02d-padded-padded-padded-padded", i);
    s.appendInbound(dm("ALICE!"), body, (uint16_t)strlen(body), 1000 + i, 2000 + i, 0, nullptr, 0);
  }
  int n = s.messageCount(dm("ALICE!"));
  ASSERT_EQ(60, n);

  struct Cap { int idx[128]; char txt[128][40]; int count; } cap{};
  s.forEachMessage(dm("ALICE!"), [](void* p, int idx, const MsgRecord& r) {
    Cap* c = (Cap*)p;
    if (c->count < 128) {
      c->idx[c->count] = idx;
      uint16_t t = r.textLen < 39 ? r.textLen : 39;
      memcpy(c->txt[c->count], r.text, t); c->txt[c->count][t] = 0;
      c->count++;
    }
  }, &cap);

  ASSERT_EQ(60, cap.count);
  for (int i = 0; i < n; i++) {
    EXPECT_EQ(i, cap.idx[i]);
    MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), i, r));
    char gm[40]; uint16_t t = r.textLen < 39 ? r.textLen : 39;
    memcpy(gm, r.text, t); gm[t] = 0;
    EXPECT_STREQ(gm, cap.txt[i]);
  }
}

// ---- index/log reconciliation after an unclean reset ----
//
// The log records are written the moment a message arrives; the index blob is
// written on a debounce (UITask). A reset inside that window keeps the records
// and loses the count increments, and messageCount() bounds every read - so the
// newest records become unreachable until later traffic pushes the count past
// them again. begin() has to reconcile the two.

static ConvoKey chan(uint8_t i) { return channelKey(i); }

// Writes `n` inbound records, flushing the index blob only after the first
// `flushAfter` of them - i.e. simulating a reset `n - flushAfter` records late.
static void seedWithStaleIndex(FakeMsgLogBackend& backend, const ConvoKey& k,
                               int flushAfter, int n) {
  MessageStore s;
  s.begin(&backend);
  s.setActiveConvo(k);            // read as they arrive, so unread stays 0
  for (int i = 0; i < n; i++) {
    char t[16]; snprintf(t, sizeof(t), "m%d", i);
    s.appendInbound(k, t, (uint16_t)strlen(t), 1000 + i, 2000 + i, 0, nullptr, 0);
    if (i + 1 == flushAfter) s.saveIndex();
  }
}

TEST(IndexReconcile, StaleIndexStillSeesEveryRecord) {
  FakeMsgLogBackend backend;
  seedWithStaleIndex(backend, chan(0), 5, 8);

  MessageStore s; s.begin(&backend);
  ASSERT_EQ(8, s.messageCount(chan(0)));
  MsgRecord r;
  ASSERT_TRUE(s.getMessage(chan(0), 7, r));
  EXPECT_EQ(0, memcmp(r.text, "m7", 2));
}

TEST(IndexReconcile, SendAfterStaleIndexIsImmediatelyVisible) {
  FakeMsgLogBackend backend;
  seedWithStaleIndex(backend, chan(0), 5, 8);

  MessageStore s; s.begin(&backend);
  s.appendOutboundChannel(chan(0), "hello", 5, 3000, 3000);
  int n = s.messageCount(chan(0));
  MsgRecord r;
  ASSERT_TRUE(s.getMessage(chan(0), n - 1, r));
  EXPECT_EQ(KIND_OUT_CHAN, r.kind);
  EXPECT_EQ(0, memcmp(r.text, "hello", 5));
}

TEST(IndexReconcile, RecoveredInboundCountsAsUnread) {
  FakeMsgLogBackend backend;
  seedWithStaleIndex(backend, chan(0), 5, 8);   // 3 records arrived after the flush

  MessageStore s; s.begin(&backend);
  ConvoSummary c;
  ASSERT_TRUE(s.getConvo(0, c));
  EXPECT_EQ(3, c.unread);
}

TEST(IndexReconcile, RecoveredOutboundIsNotUnread) {
  FakeMsgLogBackend backend;
  {
    MessageStore s; s.begin(&backend);
    s.appendInbound(chan(0), "a", 1, 1000, 1000, 0, nullptr, 0);
    s.setActiveConvo(chan(0));
    s.saveIndex();
    s.appendOutboundChannel(chan(0), "mine", 4, 2000, 2000);
  }
  MessageStore s; s.begin(&backend);
  ConvoSummary c;
  ASSERT_TRUE(s.getConvo(0, c));
  EXPECT_EQ(2, c.count);
  EXPECT_EQ(0, c.unread);
}

TEST(IndexReconcile, InSyncChatKeepsItsReadStateAndPreview) {
  FakeMsgLogBackend backend;
  {
    MessageStore s; s.begin(&backend);
    s.appendInbound(dm("ALICE!"), "read me", 7, 1000, 1000, 0, nullptr, 0);
    s.setActiveConvo(dm("ALICE!"));               // clears Alice's unread
    s.appendInbound(dm("ALICE!"), "and me", 6, 1100, 1100, 0, nullptr, 0);
    s.saveIndex();                                // Alice is in sync on disk
  }
  MessageStore s; s.begin(&backend);
  ConvoSummary c;
  ASSERT_TRUE(s.getConvo(0, c));
  EXPECT_EQ(2, c.count);
  EXPECT_EQ(0, c.unread) << "an in-sync chat must not be rescanned into unread";
  EXPECT_STREQ("and me", c.preview);
}

TEST(IndexReconcile, DriftDoesNotAccumulateAcrossResets) {
  FakeMsgLogBackend backend;
  for (int boot = 0; boot < 3; boot++) {
    MessageStore s; s.begin(&backend);
    for (int i = 0; i < 6; i++) {
      char t[16]; snprintf(t, sizeof(t), "b%dm%d", boot, i);
      s.appendInbound(chan(0), t, (uint16_t)strlen(t), 1000 + boot * 100 + i,
                      1000 + boot * 100 + i, 0, nullptr, 0);
      if (i == 3) s.saveIndex();     // last 2 of each boot never reach the blob
    }
  }
  MessageStore s; s.begin(&backend);
  EXPECT_EQ(18, s.messageCount(chan(0)));
}

TEST(IndexReconcile, LogWithNoIndexSlotIsAdopted) {
  FakeMsgLogBackend backend;
  {
    MessageStore s; s.begin(&backend);
    s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 1000, 0, nullptr, 0);
    s.saveIndex();
    s.appendInbound(chan(3), "new chat", 8, 1100, 1100, 0, nullptr, 0);  // chat born after the flush
  }
  MessageStore s; s.begin(&backend);
  EXPECT_EQ(2, s.convoCount());
  EXPECT_EQ(1, s.messageCount(chan(3)));
}

TEST(IndexReconcile, CountAheadOfLogIsClampedDown) {
  FakeMsgLogBackend backend;
  {
    MessageStore s; s.begin(&backend);
    s.appendInbound(chan(0), "a", 1, 1000, 1000, 0, nullptr, 0);
    s.appendInbound(chan(0), "b", 1, 1100, 1100, 0, nullptr, 0);
    s.saveIndex();
  }
  char name[15]; codec::keyToName(chan(0), name);
  backend.files[name].resize(0);        // log lost, blob still claims 2

  MessageStore s; s.begin(&backend);
  EXPECT_EQ(0, s.messageCount(chan(0)));
  MsgRecord r;
  EXPECT_FALSE(s.getMessage(chan(0), 0, r));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
