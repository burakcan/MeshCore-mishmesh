// test/test_mishmesh_unreadreminder/test_unread_reminder.cpp
#include <gtest/gtest.h>
#include <mishmesh/core/UnreadReminder.h>

using namespace mishmesh;

namespace {

const uint32_t NO_INPUT = 0;   // host reports 0 until the first key is dispatched

UnreadReminder every(uint16_t intervalSec, uint16_t stopAfterSec = 0) {
  UnreadReminder r;
  r.configure(intervalSec, stopAfterSec);
  return r;
}

}  // namespace

TEST(UnreadReminder, SilentUntilAnArrivalArmsIt) {
  UnreadReminder r = every(60);
  // Unread carried over from before (a reboot, say) must not start beeping.
  EXPECT_FALSE(r.tick(600000, 3, NO_INPUT));
}

TEST(UnreadReminder, NoBeepBeforeTheIntervalElapses) {
  UnreadReminder r = every(60);
  r.noteArrival(1000);
  EXPECT_FALSE(r.tick(1000, 1, NO_INPUT));
  EXPECT_FALSE(r.tick(60999, 1, NO_INPUT));
}

TEST(UnreadReminder, BeepsOncePerInterval) {
  UnreadReminder r = every(60);
  r.noteArrival(1000);
  EXPECT_TRUE(r.tick(61000, 1, NO_INPUT));
  EXPECT_FALSE(r.tick(61001, 1, NO_INPUT));
  EXPECT_FALSE(r.tick(120999, 1, NO_INPUT));
  EXPECT_TRUE(r.tick(121000, 1, NO_INPUT));
}

TEST(UnreadReminder, IntervalZeroDisablesIt) {
  UnreadReminder r = every(0);
  r.noteArrival(1000);
  EXPECT_FALSE(r.tick(3600000, 5, NO_INPUT));
}

TEST(UnreadReminder, ReadingTheChatStopsIt) {
  UnreadReminder r = every(60);
  r.noteArrival(1000);
  EXPECT_FALSE(r.tick(61000, 0, NO_INPUT));   // unread cleared: nothing to nag about
  EXPECT_FALSE(r.tick(200000, 2, NO_INPUT));  // a later unread without an arrival stays quiet
}

TEST(UnreadReminder, AnyKeyAfterArrivalSilencesIt) {
  UnreadReminder r = every(60);
  r.noteArrival(1000);
  EXPECT_FALSE(r.tick(61000, 1, 5000));       // user pressed something at t=5000
  EXPECT_FALSE(r.tick(121000, 1, 5000));      // stays silent while the same unread sits there
}

TEST(UnreadReminder, KeyPressBeforeTheArrivalDoesNotSilenceIt) {
  UnreadReminder r = every(60);
  r.noteArrival(1000);
  EXPECT_TRUE(r.tick(61000, 1, 900));         // stale timestamp from before the message landed
}

TEST(UnreadReminder, ANewArrivalReArmsAfterADismissal) {
  UnreadReminder r = every(60);
  r.noteArrival(1000);
  EXPECT_FALSE(r.tick(61000, 1, 5000));       // dismissed by a key press
  r.noteArrival(70000);                       // second message arrives
  EXPECT_FALSE(r.tick(120000, 2, 5000));      // interval restarts from the new arrival
  EXPECT_TRUE(r.tick(130000, 2, 5000));
}

TEST(UnreadReminder, RingsOutAfterStopAfter) {
  UnreadReminder r = every(60, 180);
  r.noteArrival(1000);
  EXPECT_TRUE(r.tick(61000, 1, NO_INPUT));
  EXPECT_TRUE(r.tick(121000, 1, NO_INPUT));
  EXPECT_FALSE(r.tick(181000, 1, NO_INPUT));  // past the ring-out window
  EXPECT_FALSE(r.tick(241000, 1, NO_INPUT));
}

TEST(UnreadReminder, StopAfterZeroNeverRingsOut) {
  UnreadReminder r = every(60, 0);
  r.noteArrival(1000);
  EXPECT_TRUE(r.tick(3661000, 1, NO_INPUT));  // an hour later, still going
}

TEST(UnreadReminder, ReconfiguringWhileArmedAppliesTheNewInterval) {
  UnreadReminder r = every(60);
  r.noteArrival(1000);
  EXPECT_TRUE(r.tick(61000, 1, NO_INPUT));
  r.configure(120, 0);
  EXPECT_FALSE(r.tick(121000, 1, NO_INPUT));
  EXPECT_TRUE(r.tick(181000, 1, NO_INPUT));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
