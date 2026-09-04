#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/applets/LockApplet.h>
#include <mishmesh/core/Anim.h>
#include "FakeDisplayDriver.h"

#include <vector>

using namespace mishmesh;

namespace {

// Feeds queued events; each host.loop() drains everything queued so far.
class QueueSource : public InputSource {
public:
  std::vector<InputEvent> queue;
  size_t idx = 0;
  bool poll(InputReport& out) override {
    if (idx >= queue.size()) return false;
    out.event = queue[idx++];
    out.ch = 0;
    return true;
  }
};

// A host with the real HomeApplet as root and a real LockApplet wired in. Home
// renders safely with a bare context (every service deref is guarded).
struct LockFixture {
  FakeDisplayDriver d;
  AppletHost host{&d, AppletContext{}};
  QueueSource src;
  HomeApplet home;
  LockApplet lock;

  LockFixture() {
    host.addSource(&src);
    host.setAutoOffMillis(0);        // disable auto-off unless a test opts back in
    home.setLock(&lock);
    host.setRoot(&home);
  }

  // Deliver one event at time `t` (display already on -> not swallowed as a wake).
  void press(InputEvent ev, uint32_t t) {
    src.queue.push_back(ev);
    host.loop(t);
  }
};

}  // namespace

TEST(ScreenLock, TripleBackWithinWindowLocks) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1200);
  f.press(InputEvent::Back, 1400);   // gaps < 700ms
  EXPECT_EQ(2, f.host.depth());
  EXPECT_EQ(&f.lock, f.host.foreground());
}

TEST(ScreenLock, LockPlaysFlourishThenSettlesToResting) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);          // locked at t=1200
  EXPECT_TRUE(f.lock.lockingShownForTest());   // "Locked" flourish shown

  f.host.loop(1200 + 500);                  // past the ~420ms lock animation
  EXPECT_TRUE(f.lock.lockingShownForTest());   // held until the panel blanks
  f.host.loop(1200 + 600);                  // sleep honoured -> onSleep retires it
  EXPECT_FALSE(f.lock.lockingShownForTest());
  EXPECT_FALSE(f.lock.challengeShownForTest());  // now resting (home visible)
  EXPECT_EQ(2, f.host.depth());             // still locked
}

// The flourish must never give way to a frame of bare home on its way to sleep:
// the host draws the underlay before asking this overlay, so an early retirement
// flushes one frame of unlocked-looking home.
TEST(ScreenLock, PadlockStaysUpUntilThePanelBlanks) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);

  f.host.loop(1200 + 500);
  EXPECT_TRUE(f.d.on);
  EXPECT_TRUE(f.lock.lockingShownForTest());

  f.host.loop(1200 + 600);
  EXPECT_FALSE(f.d.on);
  EXPECT_FALSE(f.lock.lockingShownForTest());
}

TEST(ScreenLock, LockingSleepsThePanel) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);
  EXPECT_TRUE(f.d.on);                      // the flourish still has to be seen

  f.host.loop(1200 + 500);                  // flourish ends, sleep is requested
  f.host.loop(1200 + 600);                  // honoured on the next pass
  EXPECT_FALSE(f.d.on);
  EXPECT_EQ(2, f.host.depth());             // still locked underneath
}

TEST(ScreenLock, WakePressCountsTowardsUnlocking) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);
  f.host.loop(1700);
  f.host.loop(1800);
  ASSERT_FALSE(f.d.on);                     // locking put the panel to sleep

  f.press(InputEvent::Back, 2000);          // wakes AND fills the first pip
  EXPECT_TRUE(f.d.on);
  EXPECT_TRUE(f.lock.challengeShownForTest());
  EXPECT_EQ(1, f.lock.pipsForTest());

  f.press(InputEvent::Back, 2100);
  f.press(InputEvent::Back, 2200);          // three presses total, same as awake
  f.host.loop(3000);                        // past the unlock animation
  EXPECT_EQ(1, f.host.depth());             // unlocked
}

TEST(ScreenLock, ReportsDeviceLockedForTheSleepFace) {
  LockFixture f;
  EXPECT_FALSE(f.host.deviceLocked());
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);
  EXPECT_TRUE(f.host.deviceLocked());

  f.press(InputEvent::Back, 1400);   // reveals AND counts as pip 1
  f.press(InputEvent::Back, 1500);
  f.press(InputEvent::Back, 1600);
  f.host.loop(2200);                 // past the unlock animation
  EXPECT_FALSE(f.host.deviceLocked());
}

TEST(ScreenLock, TripleBackLocksRatherThanSleepingFromTheFirstTap) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);
  EXPECT_EQ(2, f.host.depth());      // locked
  f.host.loop(1250);
  EXPECT_TRUE(f.d.on);               // the lock flourish still has the panel
}

TEST(ScreenLock, SlowBackDoesNotLock) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 2000);   // 1000ms gap > 700ms window
  f.press(InputEvent::Back, 3000);
  EXPECT_EQ(1, f.host.depth());
  EXPECT_EQ(&f.home, f.host.foreground());
}

TEST(ScreenLock, NonBackBreaksTheSequence) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::NavUp, 1200);  // resets the run
  f.press(InputEvent::Back, 1300);
  EXPECT_EQ(1, f.host.depth());      // only one Back since the reset
}

TEST(ScreenLock, FirstPressRevealsChallengeAndGatesHome) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);   // locked
  ASSERT_EQ(&f.lock, f.host.foreground());
  EXPECT_FALSE(f.lock.challengeShownForTest());

  f.press(InputEvent::NavDown, 1400);       // any key -> challenge (would open home drawer)
  EXPECT_TRUE(f.lock.challengeShownForTest());
  EXPECT_EQ(0, f.lock.pipsForTest());       // the reveal press does not count
  EXPECT_FALSE(f.home.drawerForTest().isOpen());   // input never reached home
}

TEST(ScreenLock, ThreeBacksFillPipsThenUnlock) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);   // locked
  f.press(InputEvent::NavRight, 1400);      // reveal the challenge
  ASSERT_TRUE(f.lock.challengeShownForTest());

  f.press(InputEvent::Back, 1500);
  EXPECT_EQ(1, f.lock.pipsForTest());
  f.press(InputEvent::Back, 1600);
  EXPECT_EQ(2, f.lock.pipsForTest());
  f.press(InputEvent::Back, 1700);          // third fill -> unlocking
  EXPECT_EQ(3, f.lock.pipsForTest());

  f.host.loop(2300);                         // past the ~450ms unlock animation
  EXPECT_EQ(1, f.host.depth());
  EXPECT_EQ(&f.home, f.host.foreground());   // unlocked, back at home
}

TEST(ScreenLock, TripleBackFromRestingUnlocks) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);   // locked, resting
  ASSERT_EQ(&f.lock, f.host.foreground());

  f.press(InputEvent::Back, 1400);   // reveals AND counts as pip 1
  EXPECT_TRUE(f.lock.challengeShownForTest());
  EXPECT_EQ(1, f.lock.pipsForTest());
  f.press(InputEvent::Back, 1500);
  f.press(InputEvent::Back, 1600);   // third -> unlocking
  EXPECT_EQ(3, f.lock.pipsForTest());

  f.host.loop(2200);                 // past the unlock animation
  EXPECT_EQ(1, f.host.depth());
  EXPECT_EQ(&f.home, f.host.foreground());
}

TEST(ScreenLock, ChallengeTimesOutBackToResting) {
  LockFixture f;
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);   // locked
  f.press(InputEvent::NavUp, 1400);         // reveal
  ASSERT_TRUE(f.lock.challengeShownForTest());

  f.host.loop(1400 + 4001);                  // > CHALLENGE_TIMEOUT_MS with no press
  EXPECT_FALSE(f.lock.challengeShownForTest());
  EXPECT_EQ(2, f.host.depth());              // still locked, just resting again
}

TEST(ScreenLock, SurvivesLongSleep) {
  LockFixture f;
  f.host.setAutoOffMillis(30000);
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);   // locked, last activity = 1200
  ASSERT_EQ(&f.lock, f.host.foreground());

  f.host.loop(1200 + 30001);                 // idle past auto-off -> sleeps
  EXPECT_FALSE(f.d.isOn());

  f.src.queue.push_back(InputEvent::Select);
  f.host.loop(1200 + 30001 + 60001);         // wake long after sleeping
  EXPECT_TRUE(f.d.isOn());
  EXPECT_EQ(2, f.host.depth());              // keepOnWake kept the lock
  EXPECT_EQ(&f.lock, f.host.foreground());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// With motion reduced the padlock ease is dropped, but the "Locked" confirmation
// itself must survive - skipping the frame outright left the screen locking with
// no feedback at all on e-ink.
TEST(ScreenLock, ReducedMotionStillHoldsTheLockedFrame) {
  LockFixture f;
  setReducedMotion(true);
  f.press(InputEvent::Back, 1000);
  f.press(InputEvent::Back, 1100);
  f.press(InputEvent::Back, 1200);          // triple-Back engages the lock
  const bool shown = f.lock.lockingShownForTest();
  f.host.loop(1200 + 2000);                 // past the 1200ms hold -> asks to sleep
  f.host.loop(1200 + 2100);                 // sleep lands; the padlock retires with it
  const bool cleared = !f.lock.lockingShownForTest();
  setReducedMotion(false);

  EXPECT_TRUE(shown);     // the confirmation is drawn, not skipped
  EXPECT_TRUE(cleared);   // and it does retire on its own
}
