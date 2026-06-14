// test/test_mishmesh_messagestore/test_messagestore.cpp
#include <gtest/gtest.h>
#include <mishmesh/core/MessageStore.h>
#include <cstring>
using namespace mishmesh;

static ConvoKey dm(const char* p) { return directKey((const uint8_t*)p); }

TEST(MessageStore, StartsEmpty) {
  MessageStore s;
  EXPECT_EQ(0, s.convoCount());
  EXPECT_EQ(0u, s.totalUnread());
}

TEST(MessageStore, InboundCreatesConvoAndMessage) {
  MessageStore s;
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

TEST(MessageStore, RecencySortedNewestFirst) {
  MessageStore s;
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

TEST(MessageStore, UnreadIncrementsWhenInactive) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "b", 1, 2, 2, 0, nullptr, 0);
  EXPECT_EQ(2u, s.totalUnread());
}

TEST(MessageStore, ActiveConvoSuppressesAndClears) {
  MessageStore s;
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

TEST(MessageStore, LastInboundTracksSubjectChat) {
  MessageStore s;
  ConvoKey k;
  EXPECT_FALSE(s.lastInbound(k));                 // nothing captured yet
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  EXPECT_TRUE(s.lastInbound(k));
  EXPECT_TRUE(k.equals(dm("ALICE!")));
  s.appendInbound(dm("BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  EXPECT_TRUE(s.lastInbound(k));
  EXPECT_TRUE(k.equals(dm("BOBBBB")));            // follows the newest arrival
}

TEST(MessageStore, ActiveConvoReportsOpenChat) {
  MessageStore s;
  ConvoKey k;
  EXPECT_FALSE(s.activeConvo(k));                 // no chat open
  s.setActiveConvo(dm("ALICE!"));
  EXPECT_TRUE(s.activeConvo(k));
  EXPECT_TRUE(k.equals(dm("ALICE!")));
  s.clearActiveConvo();
  EXPECT_FALSE(s.activeConvo(k));
}

TEST(MessageStore, PerChatCapDropsOwnOldest) {
  MessageStore s;
  for (int i = 0; i < PER_CHAT_CAP + 5; i++)
    s.appendInbound(dm("ALICE!"), "x", 1, i + 1, i + 1, 0, nullptr, 0);
  EXPECT_EQ(PER_CHAT_CAP, s.messageCount(dm("ALICE!")));
  MsgRecord r;
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(6u, r.senderTime);   // first 5 dropped, oldest remaining is #6
}

TEST(MessageStore, CompactionReclaimsTombstones) {
  MessageStore s;
  for (int i = 0; i < PER_CHAT_CAP + 50; i++)
    s.appendInbound(dm("ALICE!"), "x", 1, i + 1, i + 1, 0, nullptr, 0);
  EXPECT_EQ(PER_CHAT_CAP, s.messageCount(dm("ALICE!")));   // stays bounded, no overflow
}

TEST(MessageStore, BackstopEvictsLargestChatNotQuietDM) {
  MessageStore s;
  // Quiet DM with a unique marker, added first (oldest, smallest convo).
  s.appendInbound(dm("ALICE!"), "KEEP", 4, 1, 1, 0, nullptr, 0);

  // Fill well past ARENA_BYTES with 120-byte messages spread across several
  // channels so the per-chat caps never bound the global total. Each record is
  // ~139 bytes; 200 of them (~27 KB) far exceeds ARENA_BYTES (6144), forcing
  // the Rule-2 backstop to actually run.
  char big[120]; memset(big, 'x', sizeof(big));
  const int K = 4;
  uint32_t t = 100;
  for (int i = 0; i < 200; i++, t++)
    s.appendInbound(channelKey((uint8_t)(1 + (i % K))), big, sizeof(big), t, t, 0, nullptr, 0);

  // Final, most-recent message carries a marker (full-size so it cannot sneak
  // into any small arena gap) so we can confirm it was actually stored.
  char newest[120]; memset(newest, 'y', sizeof(newest)); memcpy(newest, "NEWEST", 6);
  s.appendInbound(channelKey(1), newest, sizeof(newest), t, t, 0, nullptr, 0);

  // (a) The quiet DM survives: count 1, it is never the largest convo, so the
  //     backstop never targets it.
  ASSERT_EQ(1, s.messageCount(dm("ALICE!")));
  MsgRecord dr; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, dr));
  EXPECT_EQ(0, memcmp(dr.text, "KEEP", 4));

  // (b) The most-recent channel message was stored — room was made for it.
  //     Distinguisher: with the stub, reserve() returns false once the arena
  //     fills and appendInbound silently drops the newest messages, so this
  //     record would be absent.
  int n = s.messageCount(channelKey(1));
  ASSERT_GT(n, 0);
  MsgRecord cr; ASSERT_TRUE(s.getMessage(channelKey(1), n - 1, cr));
  EXPECT_EQ(0, memcmp(cr.text, "NEWEST", 6));
}

TEST(MessageStore, ConvoOverflowEvictsLeastRecent) {
  MessageStore s;
  // Build a stable key for convo i: (i/20, i%20) uniquely encodes i for i<80.
  auto keyOf = [](int i) {
    char nm[7] = "C00000";
    nm[5] = (char)('A' + (i % 20)); nm[4] = (char)('0' + i / 20);
    return directKey((const uint8_t*)nm);
  };
  // MAX_CONVOS+3 distinct DMs, one message each, strictly increasing time so
  // convo i (time i+1) is less-recently-active than convo i+1.
  for (int i = 0; i < MAX_CONVOS + 3; i++)
    s.appendInbound(keyOf(i), "m", 1, i + 1, i + 1, 0, nullptr, 0);

  // Full, never over.
  EXPECT_EQ(MAX_CONVOS, s.convoCount());

  // Policy check (the distinguisher): the three least-recently-active convos
  // were evicted and the newest survive. The stub does the opposite — it drops
  // the NEWEST convos (ensureConvo returns -1), leaving the oldest intact.
  EXPECT_EQ(0, s.messageCount(keyOf(0)));                // oldest evicted
  EXPECT_EQ(0, s.messageCount(keyOf(1)));
  EXPECT_EQ(0, s.messageCount(keyOf(2)));
  EXPECT_EQ(1, s.messageCount(keyOf(MAX_CONVOS)));       // newest survive
  EXPECT_EQ(1, s.messageCount(keyOf(MAX_CONVOS + 2)));
}

TEST(MessageStore, OutboundDMStartsPendingThenDelivered) {
  MessageStore s;
  s.appendOutboundDM(dm("ALICE!"), "yo", 2, 500, 500, /*ack*/0xDEADBEEF, /*sentMs*/1000);
  MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(KIND_OUT_DM, r.kind);
  EXPECT_EQ(ST_PENDING, r.status);
  s.markDelivered(0xDEADBEEF, /*trip*/1234);
  ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(ST_DELIVERED, r.status);
  EXPECT_EQ(1234, r.tripTimeMs);
}

TEST(MessageStore, ChannelRepeatsAccumulate) {
  MessageStore s;
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

TEST(MessageStore, RepeatsCapAtMax) {
  MessageStore s;
  s.appendOutboundChannel(channelKey(2), "x", 1, 700, 700);
  uint8_t p[1] = {0x10};
  for (int i = 0; i < MAX_REPEATS + 4; i++) s.addRepeat(channelKey(2), 700, (int8_t)i, p, 1);
  EXPECT_EQ(MAX_REPEATS, s.repeatCount(channelKey(2), 0));
  MsgRecord r; ASSERT_TRUE(s.getMessage(channelKey(2), 0, r));
  EXPECT_EQ(MAX_REPEATS + 4, r.heardCount);   // count keeps climbing even past stored detail
}

TEST(MessageStore, RepeatPathSurvivesTrackingEviction) {
  MessageStore s;
  // Open MAX_TRACKED+1 distinct outbound channel messages; add a repeat with a
  // channel-distinctive path to each right after appending. The (MAX_TRACKED+1)-th
  // openTracked evicts the first-opened entry via a memmove that shifts the
  // survivors down one slot -- their repeats[].path must be re-anchored or they
  // dangle into a neighbouring slot's backing store.
  for (int c = 1; c <= MAX_TRACKED + 1; c++) {
    s.appendOutboundChannel(channelKey((uint8_t)c), "m", 1, /*senderTime*/700 + c, 700 + c);
    uint8_t path[2] = {(uint8_t)(0xA0 + c), (uint8_t)(0xB0 + c)};
    s.addRepeat(channelKey((uint8_t)c), 700 + c, /*snrx4*/(int8_t)c, path, 2);
  }
  // channel 1 was evicted; channel 2 is the oldest survivor and was the one most
  // likely to read from a stale (pre-move) address.
  RepeatRec rr; ASSERT_TRUE(s.getRepeat(channelKey(2), 0, 0, rr));
  EXPECT_EQ(2, rr.pathLen);
  EXPECT_EQ(0xA2, rr.path[0]);
  EXPECT_EQ(0xB2, rr.path[1]);
  // channel 1 fell out of tracking entirely -> summary-only.
  EXPECT_EQ(0, s.repeatCount(channelKey(1), 0));
}

TEST(MessageStore, DeleteMessageRemovesOne) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "b", 1, 2, 2, 0, nullptr, 0);
  s.deleteMessage(dm("ALICE!"), 0);
  EXPECT_EQ(1, s.messageCount(dm("ALICE!")));
  MsgRecord r; ASSERT_TRUE(s.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(0, memcmp(r.text, "b", 1));
}

TEST(MessageStore, ClearConvoKeepsEmptyChat) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  s.clearConvo(dm("ALICE!"));
  EXPECT_EQ(2, s.convoCount());                 // chat stays in the list
  EXPECT_EQ(0, s.messageCount(dm("ALICE!")));   // ...but emptied
  EXPECT_EQ(1, s.messageCount(dm("BOBBBB")));   // other chat untouched
}

TEST(MessageStore, DeletingLastMessageKeepsChat) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.deleteMessage(dm("ALICE!"), 0);
  EXPECT_EQ(1, s.convoCount());                 // chat persists even with no messages
  EXPECT_EQ(0, s.messageCount(dm("ALICE!")));
}

TEST(MessageStore, DeleteConvoRemovesChatEntirely) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.appendInbound(dm("BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  s.deleteConvo(dm("ALICE!"));
  EXPECT_EQ(1, s.convoCount());                 // ALICE gone from the list
  EXPECT_EQ(0, s.messageCount(dm("ALICE!")));
  EXPECT_EQ(1, s.messageCount(dm("BOBBBB")));   // other chat untouched
}

TEST(MessageStore, MarkUnreadSetsUnread) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  s.setActiveConvo(dm("ALICE!"));               // clears unread
  EXPECT_EQ(0u, s.totalUnread());
  s.clearActiveConvo();
  s.markUnread(dm("ALICE!"));
  EXPECT_GT(s.totalUnread(), 0u);
}

TEST(MessageStore, SerializeRoundTrip) {
  MessageStore a;
  a.appendInbound(dm("ALICE!"), "hi", 2, 1, 9, -8, (const uint8_t*)"\x5B", 1);
  a.appendOutboundChannel(channelKey(2), "yo", 2, 5, 5);
  uint8_t buf[ARENA_BYTES + 2048];
  size_t n = a.serialize(buf, sizeof(buf));
  ASSERT_GT(n, 0u);
  MessageStore b;
  ASSERT_TRUE(b.deserialize(buf, n));
  EXPECT_EQ(a.convoCount(), b.convoCount());
  EXPECT_EQ(1, b.messageCount(dm("ALICE!")));
  MsgRecord r; ASSERT_TRUE(b.getMessage(dm("ALICE!"), 0, r));
  EXPECT_EQ(-8, r.snrx4);
}

TEST(MessageStore, DeserializeRejectsGarbage) {
  MessageStore b;
  uint8_t junk[8] = {0,1,2,3,4,5,6,7};
  EXPECT_FALSE(b.deserialize(junk, sizeof(junk)));
}

TEST(MessageStore, DeserializeRejectsBadMagic) {
  MessageStore b;
  // >= 11 bytes so the length check passes; only the magic is wrong.
  uint8_t buf[16] = {0};
  memcpy(buf, "XXXX", 4); buf[4] = 1;
  EXPECT_FALSE(b.deserialize(buf, sizeof(buf)));
}

TEST(MessageStore, DeserializeRecountsDeadBytes) {
  MessageStore a;
  a.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  a.appendInbound(dm("ALICE!"), "b", 1, 2, 2, 0, nullptr, 0);
  a.appendInbound(dm("ALICE!"), "c", 1, 3, 3, 0, nullptr, 0);
  a.deleteMessage(dm("ALICE!"), 1);   // tombstone the middle record -> dead bytes in arena
  EXPECT_EQ(2, a.messageCount(dm("ALICE!")));

  uint8_t buf[ARENA_BYTES + 2048];
  size_t n = a.serialize(buf, sizeof(buf));
  ASSERT_GT(n, 0u);
  MessageStore b;
  ASSERT_TRUE(b.deserialize(buf, n));

  // Live records load correctly, tombstone excluded, in order.
  EXPECT_EQ(2, b.messageCount(dm("ALICE!")));
  MsgRecord r0, r1;
  ASSERT_TRUE(b.getMessage(dm("ALICE!"), 0, r0));
  ASSERT_TRUE(b.getMessage(dm("ALICE!"), 1, r1));
  EXPECT_EQ(0, memcmp(r0.text, "a", 1));
  EXPECT_EQ(0, memcmp(r1.text, "c", 1));

  // Behavioral proof _deadBytes was recounted: deleting the remaining live
  // records and re-walking yields a fully reclaimable, consistent store.
  b.deleteMessage(dm("ALICE!"), 0);
  b.deleteMessage(dm("ALICE!"), 0);
  EXPECT_EQ(0, b.messageCount(dm("ALICE!")));
  EXPECT_EQ(1, b.convoCount());   // chat kept even when emptied (only delete-chat removes it)
}

TEST(MessageStore, NotifiableCounterIndependentOfUnread) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.appendInbound(dm("ALICE!"), "yo", 2, 1001, 2001, 0, nullptr, 0);
  EXPECT_EQ(2u, s.totalUnread());
  EXPECT_EQ(0u, s.totalNotifyUnread());   // nothing marked notifiable yet
  s.markNotifiable(dm("ALICE!"));
  EXPECT_EQ(1u, s.totalNotifyUnread());
  EXPECT_EQ(2u, s.totalUnread());          // unread untouched by markNotifiable
}

TEST(MessageStore, OpeningChatClearsBothCounters) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.markNotifiable(dm("ALICE!"));
  s.setActiveConvo(dm("ALICE!"));          // user opens the chat
  EXPECT_EQ(0u, s.totalUnread());
  EXPECT_EQ(0u, s.totalNotifyUnread());
}

TEST(MessageStore, NotifiableSkippedForActiveConvo) {
  MessageStore s;
  s.setActiveConvo(dm("ALICE!"));
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.markNotifiable(dm("ALICE!"));          // chat is open -> no notifiable bump
  EXPECT_EQ(0u, s.totalNotifyUnread());
}

TEST(MessageStore, SerializeRoundTripsNotifyUnread) {
  MessageStore s;
  s.appendInbound(dm("ALICE!"), "hi", 2, 1000, 2000, 0, nullptr, 0);
  s.markNotifiable(dm("ALICE!"));
  uint8_t buf[8192];
  size_t n = s.serialize(buf, sizeof(buf));
  ASSERT_GT(n, 0u);
  MessageStore s2;
  ASSERT_TRUE(s2.deserialize(buf, n));
  EXPECT_EQ(1u, s2.totalNotifyUnread());
  EXPECT_EQ(1u, s2.totalUnread());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
