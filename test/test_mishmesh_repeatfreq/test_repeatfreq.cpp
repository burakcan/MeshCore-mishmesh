#include <gtest/gtest.h>
#include <mishmesh/core/RepeatFreq.h>

using namespace mishmesh;

TEST(RepeatFreq, BandIndex) {
  EXPECT_EQ(0, repeatBandIndex(433.5f));
  EXPECT_EQ(1, repeatBandIndex(869.525f));
  EXPECT_EQ(2, repeatBandIndex(915.8f));
  EXPECT_EQ(2, repeatBandIndex(918.0f));
}

TEST(RepeatFreq, PicksMatchingBand) {
  const uint32_t allowed[] = { 433000u, 869495u, 918000u };
  EXPECT_EQ(869495u, pickRepeatFreqKhz(869.525f, allowed, 3));
  EXPECT_EQ(918000u, pickRepeatFreqKhz(915.8f,  allowed, 3));
  EXPECT_EQ(433000u, pickRepeatFreqKhz(433.5f,  allowed, 3));
}

TEST(RepeatFreq, ReturnsZeroWhenNoBandMatch) {
  const uint32_t only869[] = { 869495u };
  EXPECT_EQ(0u, pickRepeatFreqKhz(915.8f, only869, 1));   // no 918-band entry
  EXPECT_EQ(0u, pickRepeatFreqKhz(915.8f, nullptr, 0));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
