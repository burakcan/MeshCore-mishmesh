#include <gtest/gtest.h>
#include <mishmesh/widgets/StepperDialog.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

TEST(StepperDialog, NavRightIncrementsClampedAtMax) {
  StepperDialog s;
  s.configure("Max hops", 63, 0, 64, nullptr);
  s.onInput(InputEvent::NavRight);
  EXPECT_EQ(64, s.value());
  s.onInput(InputEvent::NavRight);   // clamp
  EXPECT_EQ(64, s.value());
}

TEST(StepperDialog, NavLeftDecrementsClampedAtMin) {
  StepperDialog s;
  s.configure("Max hops", 1, 0, 64, nullptr);
  s.onInput(InputEvent::NavLeft);
  EXPECT_EQ(0, s.value());
  s.onInput(InputEvent::NavLeft);    // clamp
  EXPECT_EQ(0, s.value());
}

TEST(StepperDialog, SelectConfirmsWithCurrentValue) {
  StepperDialog s;
  s.configure("t", 3, 0, 64, nullptr);
  s.onInput(InputEvent::NavRight);
  EXPECT_TRUE(s.onInput(InputEvent::Select));
  EXPECT_EQ(StepperResult::Confirmed, s.result());
  EXPECT_EQ(4, s.value());
}

TEST(StepperDialog, BackCancels) {
  StepperDialog s;
  s.configure("t", 3, 0, 64, nullptr);
  EXPECT_TRUE(s.onInput(InputEvent::Back));
  EXPECT_EQ(StepperResult::Cancelled, s.result());
}

TEST(StepperDialog, ResetClears) {
  StepperDialog s;
  s.configure("t", 3, 0, 64, nullptr);
  s.onInput(InputEvent::Back);
  s.reset();
  EXPECT_EQ(StepperResult::None, s.result());
}

TEST(StepperDialog, DrawsOverlay) {
  FakeDisplayDriver d; Canvas c(&d);
  StepperDialog s;
  s.configure("Max hops", 0, 0, 64, nullptr);
  s.draw(c, 0, 0, 128, 64);
  EXPECT_GT(d.fills.size(), 10u);   // box chrome + glyph fills
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
