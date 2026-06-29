#include <gtest/gtest.h>
#include <mishmesh/core/ContactsFullLatch.h>
#include "FakeDisplayDriver.h"
#include <mishmesh/applets/ContactsFullApplet.h>
#include <mishmesh/core/AppletHost.h>

using mishmesh::ContactsFullLatch;

TEST(ContactsFullLatch, FiresOnceOnTransitionToFull) {
  ContactsFullLatch l;
  EXPECT_FALSE(l.update(99, 100, true, false));   // not full yet
  EXPECT_TRUE (l.update(100, 100, true, false));  // just became full -> fire
  EXPECT_FALSE(l.update(100, 100, true, false));  // still full -> silent
}

TEST(ContactsFullLatch, ReArmsAfterDroppingBelowFull) {
  ContactsFullLatch l;
  EXPECT_FALSE(l.update(99, 100, true, false));   // prime below full
  EXPECT_TRUE (l.update(100, 100, true, false));  // transition -> fire
  EXPECT_FALSE(l.update(100, 100, true, false));
  EXPECT_FALSE(l.update(98, 100, true, false));   // dropped -> re-arm, no fire
  EXPECT_TRUE (l.update(100, 100, true, false));  // refilled -> fire again
}

// Boot-with-already-full must NOT fire: the first update() only primes the latch,
// so a real not-full -> full transition is required before it ever fires.
TEST(ContactsFullLatch, FirstCallWhileFullDoesNotFireOnBoot) {
  ContactsFullLatch l;
  EXPECT_FALSE(l.update(100, 100, true, false));  // boot already full -> prime, no fire
  EXPECT_FALSE(l.update(100, 100, true, false));  // still full -> silent
  EXPECT_FALSE(l.update(98, 100, true, false));   // drop -> re-arm
  EXPECT_TRUE (l.update(100, 100, true, false));  // real transition -> fire
}

TEST(ContactsFullLatch, SilentWhenDisabled) {
  ContactsFullLatch l;
  EXPECT_FALSE(l.update(100, 100, false, false)); // feature off
}

TEST(ContactsFullLatch, SilentWhenOverwriteOn) {
  ContactsFullLatch l;
  EXPECT_FALSE(l.update(100, 100, true, true));   // overwrite makes room; no alert
}

TEST(ContactsFullLatch, EnablingWhileFullDoesNotRetroFire) {
  ContactsFullLatch l;
  EXPECT_FALSE(l.update(100, 100, false, false)); // full but disabled -> wasFull=true
  EXPECT_FALSE(l.update(100, 100, true, false));  // now enabled, still full -> no retro fire
  EXPECT_FALSE(l.update(98, 100, true, false));   // re-arm
  EXPECT_TRUE (l.update(100, 100, true, false));  // genuine refill -> fire
}

TEST(ContactsFullLatch, ZeroMaxNeverFull) {
  ContactsFullLatch l;
  EXPECT_FALSE(l.update(0, 0, true, false));      // max 0 -> guard, never "full"
}

TEST(ContactsFullApplet, RaiseStoresCountsAndRendersAndSwallowsInput) {
  mishmesh::contactsFullApplet().raise(100, 100);
  EXPECT_EQ(100, mishmesh::contactsFullApplet().usedForTest());
  EXPECT_EQ(100, mishmesh::contactsFullApplet().maxForTest());

  FakeDisplayDriver d;
  mishmesh::AppletContext ctx;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsFullApplet());
  host.loop(1);   // render once, must not crash (sound is null -> guarded)
  EXPECT_EQ(1, host.depth());

  // Any key is swallowed (including Back), like ClockAlertApplet.
  EXPECT_TRUE(mishmesh::contactsFullApplet().onInput(mishmesh::InputEvent::Back));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
