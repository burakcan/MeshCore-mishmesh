// test/test_mishmesh_msgspace/test_msgspace.cpp
// MessageStore on a filesystem it shares with other files: the GAT562 30S
// layout (100 KB of internal flash, 128-byte LittleFS v1 blocks) against the
// Wio Tracker L1 one (2 MB QSPI, 4096-byte blocks).
#include <gtest/gtest.h>
#include <mishmesh/core/MessageStore.h>
#include <mishmesh/core/MsgSpacePolicy.h>
#include "FakeMsgLogBackend.h"
#include <cstring>
using namespace mishmesh;

static const uint32_t BLK = 128;
static const uint32_t PARTITION = 800 * BLK;       // CustomLFS ExtraFS(0xD4000, 0x19000, 128)
static const uint32_t ADV_BLOBS = 100 * 180;       // MAX_BLOBRECS * sizeof(BlobRec)
static const uint32_t CHANNELS  = 40 * 68;         // MAX_GROUP_CHANNELS records
static const uint32_t CONTACT   = 152;

static ConvoKey dm(int i) { ConvoKey k{}; k.type = 0; k.id[0] = (uint8_t)(0x40 + i); k.id[1] = 1; return k; }

// Block counts below were read off the real LittleFS v1 code on an 800-block image.
TEST(SpacePolicy, FootprintMatchesLittleFsV1) {
  EXPECT_EQ(150u * BLK, lfsFootprint(ADV_BLOBS, BLK));
  EXPECT_EQ(23u * BLK,  lfsFootprint(CHANNELS, BLK));
  EXPECT_EQ(444u * BLK, lfsFootprint(350 * CONTACT, BLK));
  EXPECT_EQ(127u * BLK, lfsFootprint(100 * CONTACT, BLK));
  EXPECT_EQ(33u * BLK,  lfsFootprint(3911, BLK));
  EXPECT_EQ(0u, lfsFootprint(0, BLK));
  EXPECT_EQ(5u * 4096, lfsFootprint(ADV_BLOBS, 4096));
}

TEST(SpacePolicy, Gat562Numbers) {
  SharedFsUse u = { BLK, CHANNELS, CHANNELS, ADV_BLOBS };
  MsgSpacePolicy p = msgSpacePolicy(u);
  EXPECT_EQ(20u * BLK, p.reserve);            // 2560 B of slack, channels already full size
  EXPECT_EQ(150u * BLK, p.headroom);          // 19200 B: one advert blob rewrite
  EXPECT_EQ(6u * 1024, p.keep);

  u.channelsBytes = 0;                        // channels file not written yet
  EXPECT_EQ(43u * BLK, msgSpacePolicy(u).reserve);

  // The stock files alone at MAX_CONTACTS=350: 4 (fs) + 150 + 23 + 444 blocks
  // leave 179 of 800, and reserve + headroom take 170 of those. Hence `keep`.
  u.channelsBytes = CHANNELS;
  uint32_t stock = 4 * BLK + lfsFootprint(ADV_BLOBS, BLK) + lfsFootprint(CHANNELS, BLK)
                 + lfsFootprint(350 * CONTACT, BLK);
  EXPECT_EQ(179u * BLK, PARTITION - stock);
  EXPECT_LT(PARTITION - stock - p.reserve - p.headroom, p.keep);
}

TEST(SpacePolicy, UnknownBlockSizeMeansNoPolicy) {
  SharedFsUse u = { 0, 0, CHANNELS, ADV_BLOBS };
  MsgSpacePolicy p = msgSpacePolicy(u);
  EXPECT_EQ(0u, p.blockSize);
  EXPECT_EQ(0u, p.reserve); EXPECT_EQ(0u, p.headroom); EXPECT_EQ(0u, p.keep);
}

TEST(SpacePolicy, RewriteGrowthCountsOnlyNewBlocks) {
  EXPECT_EQ(2u * BLK, rewriteGrowth(100 * CONTACT, 100 * CONTACT, BLK));
  EXPECT_EQ(2u * BLK, rewriteGrowth(100 * CONTACT, 90 * CONTACT, BLK));
  EXPECT_EQ(15u * BLK, rewriteGrowth(100 * CONTACT, 110 * CONTACT, BLK));   // 127 -> 140 blocks
  EXPECT_EQ(446u * BLK, rewriteGrowth(0, 350 * CONTACT, BLK));
}

struct OpCountBackend : FakeMsgLogBackend {
  int removes = 0, copies = 0, appends = 0, frontDrops = 0;
  bool append(const char* n, const uint8_t* r, uint32_t l) override { appends++; return FakeMsgLogBackend::append(n, r, l); }
  bool remove(const char* n) override { removes++; return FakeMsgLogBackend::remove(n); }
  bool rewrite(const char* n, const uint8_t* d, uint32_t l) override { copies++; return FakeMsgLogBackend::rewrite(n, d, l); }
  bool patch(const char* n, uint32_t o, const uint8_t* d, uint32_t l) override { copies++; return FakeMsgLogBackend::patch(n, o, d, l); }
  bool dropFront(const char* n, uint32_t b) override { copies++; frontDrops++; return FakeMsgLogBackend::dropFront(n, b); }
  bool removeRange(const char* n, uint32_t o, uint32_t l) override { copies++; return FakeMsgLogBackend::removeRange(n, o, l); }
  bool truncate(const char* n, uint32_t l) override { copies++; return FakeMsgLogBackend::truncate(n, l); }
};

// GAT562 partition holding the stock files for `contacts` contacts plus a
// dozen /mm prefs files.
struct SmallFs : ::testing::Test {
  OpCountBackend backend;
  MessageStore s;
  MsgSpacePolicy policy;
  char text[80];

  void mount(int contacts) {
    backend.capacity = PARTITION;
    backend.blockSize = BLK;
    setContacts(contacts);
    SharedFsUse u = { BLK, CHANNELS, CHANNELS, ADV_BLOBS };
    policy = msgSpacePolicy(u);
    memset(text, 'm', sizeof(text));
  }
  void setContacts(int contacts) {
    backend.otherBytes = (4 + 4 + 14) * BLK + lfsFootprint(ADV_BLOBS, BLK) + lfsFootprint(CHANNELS, BLK)
                       + lfsFootprint((uint32_t)contacts * CONTACT, BLK);
  }
  void start() { s.setSpacePolicy(policy); s.begin(&backend); }
  void in(const ConvoKey& k, uint32_t t) { s.appendInbound(k, text, sizeof(text), t, t, 0, nullptr, 0); }
  uint32_t logBytes() const {
    uint32_t n = 0;
    for (auto& kv : backend.files) n += (uint32_t)kv.second.size();
    return n;
  }
  uint32_t lastTime(const ConvoKey& k) {
    MsgRecord r; int n = s.messageCount(k);
    return n > 0 && s.getMessage(k, n - 1, r) ? r.senderTime : 0;
  }
  uint32_t firstTime(const ConvoKey& k) {
    MsgRecord r;
    return s.getMessage(k, 0, r) ? r.senderTime : 0;
  }
};

// One append may dip under the line it was checked against by its own block
// rounding and a new file's directory entry, which is what the slack is for.
static const uint32_t DIP = APPEND_SLACK_BLOCKS * BLK;

TEST_F(SmallFs, EndlessTrafficNeverTakesTheHeadroom) {
  mount(100); start();
  uint32_t t = 1000;
  for (int i = 0; i < 3000; i++, t++) {
    ConvoKey k = (i % 5 == 4) ? dm(1 + (i / 5) % 6) : channelKey(0);
    in(k, t);
    ASSERT_GE(backend.freeBytes(), policy.reserve + policy.headroom - DIP) << "append " << i;
    ASSERT_EQ(t, lastTime(k)) << "append " << i;
  }
  // About 39 KB of the partition is left to the logs at 100 contacts.
  EXPECT_GT(logBytes(), 30u * 1024);
  EXPECT_LE(s.messageCount(channelKey(0)), PER_CHAT_CAP);
}

TEST_F(SmallFs, OldestChatGoesFirstThenTheBusyChatTrimsItself) {
  mount(300); start();                        // ~7 KB for the logs
  uint32_t t = 1;
  for (int c = 0; c < 3; c++)
    for (int i = 0; i < 20; i++) in(dm(c), t++);
  ASSERT_EQ(20, s.messageCount(dm(0)));
  ASSERT_EQ(20, s.messageCount(dm(2)));

  int grew = 0;
  while (s.messageCount(dm(0)) > 0 && grew < 500) { in(dm(3), t++); grew++; }
  ASSERT_LT(grew, 20);                        // gone while the newcomer was still small
  EXPECT_EQ(20, s.messageCount(dm(1)));       // newer chats are whole
  EXPECT_EQ(20, s.messageCount(dm(2)));

  uint32_t busyFrom = t;
  for (int i = 0; i < 400; i++) in(dm(3), t++);
  EXPECT_EQ(20, s.messageCount(dm(1)));       // the busy chat did not push them out
  EXPECT_EQ(20, s.messageCount(dm(2)));
  EXPECT_GT(s.messageCount(dm(3)), 16);       // it lost its own oldest instead
  EXPECT_LT(s.messageCount(dm(3)), PER_CHAT_CAP);
  EXPECT_GT(firstTime(dm(3)), busyFrom);
  EXPECT_EQ(t - 1, lastTime(dm(3)));
  EXPECT_GE(backend.freeBytes(), policy.reserve + policy.headroom - DIP);
}

TEST_F(SmallFs, EvictionLeavesChatsWithoutALogListed) {
  mount(300); start();
  s.ensureChannel(3);                         // joined, nothing received yet
  uint32_t t = 1;
  for (int i = 0; i < 50; i++) in(dm(0), t++);
  for (int i = 0; i < 400; i++) in(dm(1), t++);
  EXPECT_EQ(0, s.messageCount(dm(0)));
  bool listed = false;
  for (int i = 0; i < s.convoCount(); i++) {
    ConvoSummary c;
    if (s.getConvo(i, c) && c.key.equals(channelKey(3))) listed = true;
  }
  EXPECT_TRUE(listed);
}

// At MAX_CONTACTS the stock files leave less than reserve + headroom, so the
// headroom is given up for the last `keep` bytes of messages. The reserve is not.
TEST_F(SmallFs, FullContactListStillKeepsTheLatestMessages) {
  mount(350); start();
  ASSERT_LT(backend.freeBytes(), policy.reserve + policy.headroom);
  uint32_t t = 1;
  for (int i = 0; i < 5; i++) in(dm(1), t++);
  for (int i = 0; i < 1500; i++, t++) {
    in(channelKey(0), t);
    ASSERT_GE(backend.freeBytes(), policy.reserve - DIP) << "append " << i;
    ASSERT_LE(logBytes(), policy.keep) << "append " << i;
    ASSERT_EQ(t, lastTime(channelKey(0))) << "append " << i;
  }
  EXPECT_GT(logBytes(), policy.keep / 2);
  EXPECT_EQ(5, s.messageCount(dm(1)));
}

TEST_F(SmallFs, NoRoomAtAllDropsTheMessageWithoutWriting) {
  mount(350);
  backend.otherBytes = PARTITION - policy.reserve / 2;
  start();
  in(dm(0), 1);
  EXPECT_EQ(0, s.convoCount());
  EXPECT_EQ(0, backend.appends + backend.copies + backend.removes);
}

TEST_F(SmallFs, FullPartitionFromOlderFirmwareRecoversAtBoot) {
  mount(200);
  {
    MessageStore old;                         // no policy: fills the partition
    old.begin(&backend);
    uint32_t t = 1;
    for (int i = 0; backend.freeBytes() > 2 * BLK && i < 5000; i++, t++)
      old.appendInbound(dm(i % 20), text, sizeof(text), t, t, 0, nullptr, 0);
    old.saveIndex();
    ASSERT_LE(backend.freeBytes(), 2 * BLK);
    ASSERT_EQ(20, old.convoCount());
  }
  backend.removes = backend.copies = backend.appends = 0;
  start();
  EXPECT_GT(backend.removes, 0);
  EXPECT_LE(backend.removes, 8);              // whole chats, a bounded number
  EXPECT_EQ(0, backend.copies);               // and no log is rewritten
  EXPECT_EQ(0, backend.appends);
  EXPECT_EQ(20 - backend.removes, s.convoCount());
  EXPECT_EQ(0, s.messageCount(dm(0)));        // oldest went first
  EXPECT_GE(backend.freeBytes(), policy.reserve);
  EXPECT_LT(backend.freeBytes(), policy.reserve + policy.headroom);   // boot stops at the reserve

  in(dm(19), 9000);                           // the first write restores the headroom
  EXPECT_GE(backend.freeBytes(), policy.reserve + policy.headroom - DIP);
  EXPECT_EQ(9000u, lastTime(dm(19)));
}

// A few contacts auto-added since the last append leave the partition a couple
// of blocks short of the headroom. That is not worth a chat at the next boot.
TEST_F(SmallFs, RebootSlightlyUnderTheHeadroomDeletesNothing) {
  mount(100); start();
  uint32_t t = 1;
  for (int i = 0; i < 2000; i++, t++) in(dm(i % 8), t);
  s.saveIndex();
  int convos = s.convoCount();
  uint32_t line = policy.reserve + policy.headroom;
  backend.otherBytes += backend.freeBytes() - line + 2 * BLK;   // 256 B short
  ASSERT_EQ(line - 2 * BLK, backend.freeBytes());

  backend.removes = backend.copies = 0;
  MessageStore s2;
  s2.setSpacePolicy(policy);
  s2.begin(&backend);
  EXPECT_EQ(0, backend.removes);
  EXPECT_EQ(0, backend.copies);
  EXPECT_EQ(convos, s2.convoCount());

  s2.appendInbound(dm(7), text, sizeof(text), t, t, 0, nullptr, 0);
  EXPECT_GE(backend.freeBytes(), line - DIP);  // the next append makes it up
}

// A failed free-space scan must not read as a full partition.
TEST_F(SmallFs, UnknownFreeSpaceDeletesNothing) {
  mount(350); start();
  uint32_t t = 1;
  for (int c = 0; c < 4; c++)
    for (int i = 0; i < 10; i++) in(dm(c), t++);
  s.saveIndex();
  backend.otherBytes = PARTITION - backend.usedBytes() + backend.otherBytes;   // truly full
  backend.freeUnknown = true;
  backend.removes = backend.copies = 0;

  for (int i = 0; i < 5; i++) in(dm(0), t++);          // appends fail, nothing else happens
  s.makeRoom(rewriteGrowth(0, 350 * CONTACT, BLK));     // contacts-save hook
  MessageStore s2;
  s2.setSpacePolicy(policy);
  s2.begin(&backend);                                   // reboot
  EXPECT_EQ(0, backend.removes);
  EXPECT_EQ(0, backend.copies);
  EXPECT_EQ(4, s.convoCount());
  EXPECT_EQ(4, s2.convoCount());
  for (int c = 0; c < 4; c++) EXPECT_EQ(10, s2.messageCount(dm(c)));
}

TEST_F(SmallFs, LogsGiveWayToAGrowingContactsFile) {
  mount(100); start();
  uint32_t t = 1;
  for (int i = 0; i < 2000; i++, t++) in(dm(i % 8), t);
  int before = s.convoCount();
  uint32_t seq = s.seq();

  // 250 contacts imported in one go: the file grows by about 40 KB.
  uint32_t grow = rewriteGrowth(100 * CONTACT, 350 * CONTACT, BLK);
  backend.removes = backend.copies = 0;
  s.makeRoom(grow);
  EXPECT_GE(backend.freeBytes(), policy.reserve + grow);
  EXPECT_LT(s.convoCount(), before);
  EXPECT_GT(backend.removes, 0);
  EXPECT_EQ(0, backend.copies);
  EXPECT_NE(seq, s.seq());                    // index blob gets rewritten
  EXPECT_EQ(t - 1, lastTime(dm(7)));          // newest chat is the last to go

  setContacts(350);                           // the save lands
  EXPECT_GE(backend.freeBytes(), policy.reserve);
  in(dm(7), t);                               // and messages carry on in what is left
  EXPECT_EQ(t, lastTime(dm(7)));
}

TEST_F(SmallFs, MakeRoomIsANoOpWhenThereIsRoom) {
  mount(100); start();
  for (uint32_t t = 1; t <= 50; t++) in(dm(0), t);
  uint32_t seq = s.seq();
  s.makeRoom(rewriteGrowth(100 * CONTACT, 101 * CONTACT, BLK));
  EXPECT_EQ(50, s.messageCount(dm(0)));
  EXPECT_EQ(seq, s.seq());
}

// Something else ate into the reserve and the copy a trim needs does not fit.
// Emptying the log needs no room at all.
TEST_F(SmallFs, TrimWithoutRoomForTheCopyStartsTheLogOver) {
  mount(350);
  policy.keep = 0; policy.headroom = 0;
  start();
  uint32_t t = 1;
  for (int i = 0; i < 40; i++) in(dm(0), t++);
  ASSERT_EQ(40, s.messageCount(dm(0)));
  backend.otherBytes += backend.freeBytes() - policy.reserve / 2;
  backend.copies = 0;
  in(dm(0), t);
  EXPECT_EQ(1, s.messageCount(dm(0)));
  EXPECT_EQ(t, lastTime(dm(0)));
  EXPECT_EQ(1, s.convoCount());
  EXPECT_EQ(1, backend.frontDrops);           // the emptying; no copy was tried first
}

// With the headroom given up there may be no room to copy a log's tail. The
// trim then waits, as long as appends still leave the reserve alone.
TEST_F(SmallFs, TrimThatCannotCopyWaitsWhileTheReserveHolds) {
  mount(350); start();
  uint32_t t = 1;
  while (logBytes() + 200 < policy.keep) in(dm(0), t++);
  int n = s.messageCount(dm(0));
  backend.otherBytes += backend.freeBytes() - policy.reserve - 8 * BLK;
  backend.frontDrops = 0;
  for (int i = 0; i < 3; i++) in(dm(0), t++);
  EXPECT_EQ(0, backend.frontDrops);
  EXPECT_EQ(n + 3, s.messageCount(dm(0)));
  EXPECT_EQ(t - 1, lastTime(dm(0)));
  for (int i = 0; i < 20; i++) {
    in(dm(0), t++);
    ASSERT_GE(backend.freeBytes(), policy.reserve - DIP);
    ASSERT_EQ(t - 1, lastTime(dm(0)));
  }
  EXPECT_LT(s.messageCount(dm(0)), n);        // emptied once the reserve was reached
}

// A log over PER_CHAT_CAP would hide its newest messages from the thread view,
// so a rotation short of room for its copy takes it from the least-recent chat.
TEST_F(SmallFs, RotationShortOfRoomEvictsAnOlderChat) {
  mount(20);
  policy.headroom = 0; policy.keep = 0;       // let the logs run the partition down
  start();
  uint32_t t = 1;
  for (int i = 0; i < 60; i++) in(dm(1), t++);
  for (int i = 0; i < 30; i++) in(dm(2), t++);
  for (int i = 0; i < PER_CHAT_CAP; i++) in(dm(0), t++);
  // Room for appends, not for a copy of dm(0)'s tail (72 blocks) until dm(1)'s 50 come back.
  backend.otherBytes += backend.freeBytes() - policy.reserve - 40 * BLK;
  in(dm(0), t++);
  EXPECT_EQ(PER_CHAT_CAP - 16 + 1, s.messageCount(dm(0)));
  EXPECT_EQ(0, s.messageCount(dm(1)));        // least recent, gone
  EXPECT_EQ(30, s.messageCount(dm(2)));       // one was enough
  EXPECT_EQ(t - 1, lastTime(dm(0)));
}

// With nothing else to free the log starts over. No copy is attempted first:
// it would write most of the log before failing.
TEST_F(SmallFs, RotationWithNothingToEvictStartsTheLogOver) {
  mount(20);
  policy.headroom = 0; policy.keep = 0;
  start();
  uint32_t t = 1;
  for (int i = 0; i < PER_CHAT_CAP; i++) in(dm(0), t++);
  backend.otherBytes += backend.freeBytes() - policy.reserve - 40 * BLK;
  backend.copies = backend.frontDrops = 0;
  for (int i = 0; i < 16; i++) {
    in(dm(0), t++);
    ASSERT_LE(s.messageCount(dm(0)), PER_CHAT_CAP);
  }
  EXPECT_EQ(16, s.messageCount(dm(0)));
  EXPECT_EQ(1, backend.copies);               // the emptying
  EXPECT_EQ(t - 1, lastTime(dm(0)));
}

// Same traffic with and without the policy on a QSPI-sized filesystem: the
// policy never binds there, MSG_FLASH_BUDGET stays the only limit.
TEST(LargeFs, PolicyChangesNothingOnQspi) {
  SharedFsUse u = { 4096, 0, CHANNELS, ADV_BLOBS };
  MsgSpacePolicy policy = msgSpacePolicy(u);
  EXPECT_EQ(4096u, policy.blockSize);
  EXPECT_EQ((1u + 20u) * 4096, policy.reserve);
  EXPECT_EQ(5u * 4096, policy.headroom);

  FakeMsgLogBackend with, without;
  for (FakeMsgLogBackend* b : { &with, &without }) {
    b->capacity = 2u * 1024 * 1024;
    b->blockSize = 4096;
    b->otherBytes = lfsFootprint(ADV_BLOBS, 4096) + lfsFootprint(CHANNELS, 4096)
                  + lfsFootprint(350 * CONTACT, 4096) + 40 * 4096;
  }
  MessageStore a, b;
  a.setSpacePolicy(policy);
  a.begin(&with); b.begin(&without);

  char big[MAX_TEXT]; memset(big, 'x', sizeof(big));
  uint8_t path[MAX_PATH_BYTES] = {0};
  const uint8_t threeByteHops = (2 << 6) | MAX_PATH;
  uint32_t t = 1;
  // 64 chats x 100 messages of the largest record is over the 1200 KB budget.
  for (int round = 0; round < PER_CHAT_CAP; round++)
    for (int c = 0; c < MAX_CONVOS; c++, t++) {
      ConvoKey k = dm(c);
      a.appendInbound(k, big, sizeof(big), t, t, 0, path, threeByteHops);
      b.appendInbound(k, big, sizeof(big), t, t, 0, path, threeByteHops);
    }
  ASSERT_EQ(b.convoCount(), a.convoCount());
  uint32_t kept = 0;
  for (int i = 0; i < a.convoCount(); i++) {
    ConvoSummary ca, cb;
    ASSERT_TRUE(a.getConvo(i, ca)); ASSERT_TRUE(b.getConvo(i, cb));
    EXPECT_TRUE(ca.key.equals(cb.key));
    EXPECT_EQ(cb.count, ca.count);
    EXPECT_EQ(cb.logBytes, ca.logBytes);
    kept += ca.logBytes;
  }
  const uint32_t budget = MessageStore::MSG_FLASH_BUDGET;
  EXPECT_LE(kept, budget);
  EXPECT_GT(kept, budget - 64u * 1024);       // the budget did bind
  EXPECT_EQ(without.files, with.files);
  EXPECT_GT(with.freeBytes(), policy.reserve + policy.headroom);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
