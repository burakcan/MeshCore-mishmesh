#include <gtest/gtest.h>
#include <mishmesh/core/ScreenSleep.h>
#include <mishmesh/core/NameValidation.h>

using namespace mishmesh;

TEST(ScreenSleep, MillisLadder) {
  EXPECT_EQ(15000u,  screenSleepMillis(0));
  EXPECT_EQ(30000u,  screenSleepMillis(1));
  EXPECT_EQ(60000u,  screenSleepMillis(2));
  EXPECT_EQ(120000u, screenSleepMillis(3));
  EXPECT_EQ(300000u, screenSleepMillis(4));
  EXPECT_EQ(0u,      screenSleepMillis(5));      // Never = auto-off disabled
  EXPECT_EQ(30000u,  screenSleepMillis(99));     // out-of-range -> 30s default
}

TEST(ScreenSleep, Labels) {
  EXPECT_STREQ("15s",   screenSleepLabel(0));
  EXPECT_STREQ("30s",   screenSleepLabel(1));
  EXPECT_STREQ("1m",    screenSleepLabel(2));
  EXPECT_STREQ("2m",    screenSleepLabel(3));
  EXPECT_STREQ("5m",    screenSleepLabel(4));
  EXPECT_STREQ("Never", screenSleepLabel(5));
  EXPECT_STREQ("30s",   screenSleepLabel(-1));   // out-of-range -> default
  EXPECT_EQ(6, SCREEN_SLEEP_COUNT);
}

TEST(ScreenSleep, StoredEncodingRoundTrips) {
  for (int i = 0; i < SCREEN_SLEEP_COUNT; i++)
    EXPECT_EQ(i, screenSleepStoredToIndex(screenSleepIndexToStored(i)));
}

TEST(ScreenSleep, ZeroStoredIsThirtySecondDefault) {
  EXPECT_EQ(1, screenSleepStoredToIndex(0));     // legacy/unset -> 30s index
  EXPECT_EQ(30000u, screenSleepMillis(screenSleepStoredToIndex(0)));
}

TEST(NodeName, RejectsEmptyAndBadChars) {
  EXPECT_FALSE(isValidNodeName(nullptr));
  EXPECT_FALSE(isValidNodeName(""));
  EXPECT_FALSE(isValidNodeName("a,b"));
  EXPECT_FALSE(isValidNodeName("bad*"));
  EXPECT_FALSE(isValidNodeName("[x]"));
  EXPECT_FALSE(isValidNodeName("a:b"));
  EXPECT_FALSE(isValidNodeName("a?b"));
  EXPECT_FALSE(isValidNodeName("a\\b"));
  EXPECT_TRUE(isValidNodeName("Node-1"));
  EXPECT_TRUE(isValidNodeName("My Radio"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
