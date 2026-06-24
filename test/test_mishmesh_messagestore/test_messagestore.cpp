// test/test_mishmesh_messagestore/test_messagestore.cpp
#include <gtest/gtest.h>
#include <mishmesh/core/MessageStore.h>
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
