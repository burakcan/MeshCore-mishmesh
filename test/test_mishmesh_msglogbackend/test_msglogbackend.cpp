#include <gtest/gtest.h>
#include "FakeMsgLogBackend.h"
using namespace mishmesh;

TEST(FakeMsgLogBackend, AppendReadRoundTrip) {
  FakeMsgLogBackend b;
  uint8_t rec[3] = {1, 2, 3};
  ASSERT_TRUE(b.append("0011223344556", rec, 3));
  EXPECT_EQ(3u, b.size("0011223344556"));
  uint8_t buf[8] = {0};
  EXPECT_EQ(2u, b.read("0011223344556", 1, buf, 8));
  EXPECT_EQ(2, buf[0]);
  EXPECT_EQ(3, buf[1]);
}

TEST(FakeMsgLogBackend, ListAndRemove) {
  FakeMsgLogBackend b;
  uint8_t r = 9;
  b.append("aaaa", &r, 1);
  b.append("bbbb", &r, 1);
  MsgLogInfo info[4];
  EXPECT_EQ(2, b.list(info, 4));
  EXPECT_TRUE(b.remove("aaaa"));
  EXPECT_EQ(1, b.list(info, 4));
}

TEST(FakeMsgLogBackend, CapacityBlocksAppend) {
  FakeMsgLogBackend b;
  b.capacity = 4;
  uint8_t rec[3] = {1, 2, 3};
  ASSERT_TRUE(b.append("x", rec, 3));   // 3 <= 4
  EXPECT_FALSE(b.append("x", rec, 3));  // 6 > 4
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
