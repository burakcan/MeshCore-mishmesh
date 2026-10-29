#include <gtest/gtest.h>
#include <mishmesh/core/CliReplyRing.h>
#include <string>

using namespace mishmesh;

namespace {

const uint8_t A[6] = {0xA0, 1, 2, 3, 4, 5};
const uint8_t B[6] = {0xB0, 1, 2, 3, 4, 5};

}  // namespace

TEST(CliReplyRing, EmptyRingHasNoReplyAndSeqZero) {
  CliReplyRing r;
  const uint8_t zero[6] = {0};
  EXPECT_EQ(0u, r.seq());
  EXPECT_EQ(nullptr, r.newest(A, 0));
  EXPECT_EQ(nullptr, r.newest(zero, 0));   // blank slots must not match a zero key
}

TEST(CliReplyRing, EveryReplyBumpsSeqByOne) {
  CliReplyRing r;
  r.latch(A, "one");
  EXPECT_EQ(1u, r.seq());
  r.latch(B, "two");
  EXPECT_EQ(2u, r.seq());
}

TEST(CliReplyRing, ReplyIsAttributedToItsSender) {
  CliReplyRing r;
  r.latch(A, "> 37.7");
  EXPECT_STREQ("> 37.7", r.newest(A, 0));
  EXPECT_EQ(nullptr, r.newest(B, 0));
}

TEST(CliReplyRing, OnlyRepliesNewerThanTheSnapshotCount) {
  CliReplyRing r;
  r.latch(A, "stale");
  const uint32_t fired = r.seq();
  EXPECT_EQ(nullptr, r.newest(A, fired));
  r.latch(A, "fresh");
  EXPECT_STREQ("fresh", r.newest(A, fired));
}

TEST(CliReplyRing, NewestOfSeveralFromTheSameSenderWins) {
  CliReplyRing r;
  r.latch(A, "first");
  r.latch(A, "second");
  EXPECT_STREQ("second", r.newest(A, 0));
}

TEST(CliReplyRing, AnotherSendersReplyDoesNotClobberAWaitingOne) {
  CliReplyRing r;
  r.latch(A, "mine");
  for (int i = 0; i < CliReplyRing::SLOTS - 1; i++) r.latch(B, "theirs");
  EXPECT_STREQ("mine", r.newest(A, 0));
  r.latch(B, "theirs");                    // the ring has now gone all the way round
  EXPECT_EQ(nullptr, r.newest(A, 0));
}

TEST(CliReplyRing, OverlongReplyIsCutToTheFrameSize) {
  CliReplyRing r;
  std::string big(400, 'x');
  r.latch(A, big.c_str());
  EXPECT_EQ((size_t)CliReplyRing::REPLY_MAX - 1, strlen(r.newest(A, 0)));
}

TEST(CliReplyRing, NullTextLatchesAnEmptyReply) {
  CliReplyRing r;
  r.latch(A, nullptr);
  ASSERT_NE(nullptr, r.newest(A, 0));
  EXPECT_STREQ("", r.newest(A, 0));
  EXPECT_EQ(1u, r.seq());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
