#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/applets/LockApplet.h>
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
  EXPECT_FALSE(f.lock.lockingShownForTest());
  EXPECT_FALSE(f.lock.challengeShownForTest());  // now resting (home visible)
  EXPECT_EQ(2, f.host.depth());             // still locked
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
