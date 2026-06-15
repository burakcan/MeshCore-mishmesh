#include <gtest/gtest.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

TEST(ConfirmDialog, DefaultsToConfirmSelected) {
  ConfirmDialog dlg;
  dlg.configure("Delete Alice?");
  EXPECT_EQ(ConfirmResult::None, dlg.result());
  // Select with default selection (Confirm) confirms.
  EXPECT_TRUE(dlg.onInput(InputEvent::Select));
  EXPECT_EQ(ConfirmResult::Confirmed, dlg.result());
}

TEST(ConfirmDialog, NavLeftThenSelectCancels) {
  ConfirmDialog dlg;
  dlg.configure("Delete Alice?");
  dlg.onInput(InputEvent::NavLeft);      // move to Cancel
  EXPECT_TRUE(dlg.onInput(InputEvent::Select));
  EXPECT_EQ(ConfirmResult::Cancelled, dlg.result());
}

TEST(ConfirmDialog, BackCancels) {
  ConfirmDialog dlg;
  dlg.configure("msg");
  EXPECT_TRUE(dlg.onInput(InputEvent::Back));
  EXPECT_EQ(ConfirmResult::Cancelled, dlg.result());
}

TEST(ConfirmDialog, ResetClears) {
  ConfirmDialog dlg;
  dlg.configure("y");
  dlg.onInput(InputEvent::Back);
  dlg.reset();
  EXPECT_EQ(ConfirmResult::None, dlg.result());
}

TEST(ConfirmDialog, DrawsOverlayScrim) {
  FakeDisplayDriver d; Canvas c(&d);
  ConfirmDialog dlg;
  dlg.configure("Delete Alice?");
  dlg.draw(c, 0, 0, 128, 64);
  EXPECT_GT(d.fills.size(), 10u);   // scrim stipple + box
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
