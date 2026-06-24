#include <gtest/gtest.h>
#include <mishmesh/core/MessageStore.h>   // ConvoKey, directKey
#include <mishmesh/core/MsgCodec.h>
#include <cstring>
using namespace mishmesh;

TEST(MsgCodec, KeyNameRoundTrip) {
  ConvoKey k = directKey((const uint8_t*)"ALICE!");
  char name[15];
  codec::keyToName(k, name);
  EXPECT_EQ(14u, std::strlen(name));
  ConvoKey back;
  ASSERT_TRUE(codec::nameToKey(name, back));
  EXPECT_TRUE(k.equals(back));
}

TEST(MsgCodec, WriteRecordThenReadBack) {
  ConvoKey k = directKey((const uint8_t*)"BOBBBB");
  uint8_t trailer[3] = { /*status*/1, /*trip lo*/0x10, /*trip hi*/0x00 };
  uint8_t rec[codec::MAX_REC];
  int n = codec::writeRecord(rec, KIND_OUT_DM, k, "hey", 3, 111, 222, trailer, 3);
  EXPECT_EQ(codec::REC_HDR + 3 + 3, n);
  EXPECT_EQ(n, codec::recSize(rec));
  EXPECT_EQ(KIND_OUT_DM, codec::recKind(rec));
  EXPECT_EQ(3u, codec::recTextLen(rec));
  EXPECT_EQ(222u, codec::rd32(rec + 12));
  ConvoKey rk; codec::rdKey(rec, rk);
  EXPECT_TRUE(k.equals(rk));
}

TEST(MsgCodec, NameToKeyRejectsBadInput) {
  ConvoKey k;
  EXPECT_FALSE(codec::nameToKey("short", k));
  EXPECT_FALSE(codec::nameToKey("zz112233445566", k)); // non-hex
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
