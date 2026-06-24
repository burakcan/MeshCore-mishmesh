#include <gtest/gtest.h>
#include <mishmesh/core/MessageStore.h>
#include <mishmesh/core/ConvoIndex.h>
#include <cstring>
using namespace mishmesh;
static ConvoKey dm(const char* p){ return directKey((const uint8_t*)p); }

TEST(ConvoIndex, EnsureFindTouchRecencySort) {
  ConvoIndex ix;
  ix.ensure(dm("ALICE!")); ix.touch(ix.find(dm("ALICE!")), 100);
  ix.ensure(dm("BOBBBB")); ix.touch(ix.find(dm("BOBBBB")), 200);
  ASSERT_EQ(2, ix.count());
  ConvoSummary c0; ASSERT_TRUE(ix.get(0, c0));
  EXPECT_TRUE(c0.key.equals(dm("BOBBBB")));   // newest first
}

TEST(ConvoIndex, PreviewStoredAndTruncated) {
  ConvoIndex ix;
  ix.ensure(dm("ALICE!"));
  ix.setPreview(dm("ALICE!"), "hello world this is a very long message body", 44);
  const char* p = ix.preview(dm("ALICE!"));
  ASSERT_NE(nullptr, p);
  EXPECT_LT(std::strlen(p), (size_t)PREVIEW_LEN);
}

TEST(ConvoIndex, SerializeDeserializeRoundTrip) {
  ConvoIndex a;
  ConvoSummary& s = a.ensure(dm("ALICE!"));
  s.lastTime = 500; s.unread = 3; s.count = 12; s.logBytes = 999;
  a.setPreview(dm("ALICE!"), "hi", 2);
  uint8_t buf[512];
  uint32_t n = a.serialize(buf, sizeof(buf));
  ASSERT_GT(n, 0u);
  ConvoIndex b;
  ASSERT_TRUE(b.deserialize(buf, n));
  ASSERT_EQ(1, b.count());
  EXPECT_EQ(0, std::strcmp(b.preview(dm("ALICE!")), "hi"));
  ConvoSummary got; ASSERT_TRUE(b.get(0, got));
  EXPECT_EQ(999u, got.logBytes);
  EXPECT_EQ(3u, got.unread);
}

TEST(ConvoIndex, EvictLRUReportsVictim) {
  ConvoIndex ix;
  ix.ensure(dm("AAAAAA")); ix.touch(ix.find(dm("AAAAAA")), 10);
  ix.ensure(dm("BBBBBB")); ix.touch(ix.find(dm("BBBBBB")), 20);
  ConvoKey victim;
  ASSERT_TRUE(ix.evictLRU(victim));
  EXPECT_TRUE(victim.equals(dm("AAAAAA")));   // oldest lastTime
  EXPECT_EQ(1, ix.count());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
