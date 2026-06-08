#include <gtest/gtest.h>
#include <mishmesh/arduboy/ArduboyGate.h>
#include <mishmesh/core/InputEvent.h>

using namespace mishmesh;
using namespace mishmesh::arduboy;

TEST(ArduboyGate, StepDueGatesTo60HzNoCatchup) {
  ArduboyGate g;
  EXPECT_TRUE(g.stepDue(0));        // first tick always due
  EXPECT_FALSE(g.stepDue(5));       // 5ms later: not due
  EXPECT_FALSE(g.stepDue(15));      // 15ms: not due (<16)
  EXPECT_TRUE(g.stepDue(16));       // 16ms: due, resets to 16
  EXPECT_FALSE(g.stepDue(20));
  // Long gap does NOT produce multiple dues (no catch-up): one true, then not-due.
  EXPECT_TRUE(g.stepDue(200));
  EXPECT_FALSE(g.stepDue(205));
}

TEST(ArduboyGate, HeldDirectionsMapToButtonMask) {
  ArduboyGate g;
  InputState in;
  in.held = maskBit(InputEvent::NavLeft) | maskBit(InputEvent::NavUp);
  uint8_t m = g.buttonMask(in);
  EXPECT_TRUE(m & ArduboyGate::LEFT);
  EXPECT_TRUE(m & ArduboyGate::UP);
  EXPECT_FALSE(m & ArduboyGate::RIGHT);
  EXPECT_FALSE(m & ArduboyGate::A_B);
}

TEST(ArduboyGate, PressABLatchesForOneRead) {
  ArduboyGate g;
  InputState in;                    // nothing held
  g.pressAB();
  EXPECT_TRUE(g.buttonMask(in) & ArduboyGate::A_B);   // first read sees it
  EXPECT_FALSE(g.buttonMask(in) & ArduboyGate::A_B);  // latch cleared
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
