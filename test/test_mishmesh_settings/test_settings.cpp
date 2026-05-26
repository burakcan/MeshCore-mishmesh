#include <gtest/gtest.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Toggle.h>
#include <mishmesh/widgets/Toast.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

TEST(Toast, ActiveWhileWithinDurationThenExpires) {
  Toast t;
  EXPECT_FALSE(t.active(0));
  t.show("Path reset", 1000, 1400);
  EXPECT_TRUE(t.active(1000));
  EXPECT_TRUE(t.active(2399));
  EXPECT_FALSE(t.active(2400));   // expired at now >= until
}

TEST(Toast, DrawsBar) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Toast t;
  t.show("Cleared", 0);
  t.draw(c, 0, 50, 128, 14);
  EXPECT_GT(d.fills.size(), 0u);
}

namespace {
struct ValModel : ListModel {
  int count() const override { return 2; }
  const char* label(int i) const override { return i == 0 ? "Users" : "Rooms"; }
  const char* value(int i) const override { return i == 0 ? "ON" : "OFF"; }
};
struct ToggleModel : ListModel {
  int count() const override { return 2; }
  const char* label(int i) const override { return i == 0 ? "Users" : "Rooms"; }
  bool isToggle(int) const override { return true; }
  bool toggleState(int i) const override { return i == 0; }
};
}

TEST(ListMenuMarquee, NeedsAnimationOnlyWhenSelectedOverflows) {
  struct LongModel : ListModel {
    int count() const override { return 1; }
    const char* label(int) const override { return "A very long contact name that overflows"; }
  };
  struct ShortModel : ListModel {
    int count() const override { return 1; }
    const char* label(int) const override { return "Bob"; }
  };
  FakeDisplayDriver d; Canvas c(&d);
  LongModel lm; ListMenu m1; m1.setModel(&lm);
  m1.draw(c, 0, 0, 64, 14);          // narrow box -> selected row 0 overflows
  EXPECT_TRUE(m1.needsAnimation());
  ShortModel sm; ListMenu m2; m2.setModel(&sm);
  m2.draw(c, 0, 0, 128, 14);
  EXPECT_FALSE(m2.needsAnimation());
}

TEST(ListMenuToggle, DrawsTogglePillRows) {
  FakeDisplayDriver d;
  Canvas c(&d);
  ToggleModel m;
  ListMenu menu;
  menu.setModel(&m);
  menu.draw(c, 0, 0, 128, 48);   // selected row 0 renders an ON pill in DARK on its LIGHT bar
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(ListMenuValue, DrawsValueColumn) {
  FakeDisplayDriver d;
  Canvas c(&d);
  ValModel m;
  ListMenu menu;
  menu.setModel(&m);
  menu.draw(c, 0, 0, 128, 48);
  // "ON"/"OFF" rendered => some glyph pixel runs beyond the label area.
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(ListMenuValue, DrawsMoreFillsWithValue) {
  struct NullModel : ListModel {
    int count() const override { return 2; }
    const char* label(int i) const override { return i == 0 ? "Users" : "Rooms"; }
  };
  FakeDisplayDriver d_null, d_val;
  Canvas c_null(&d_null), c_val(&d_val);
  NullModel nm;  ValModel vm;
  ListMenu menu_null, menu_val;
  menu_null.setModel(&nm);  menu_val.setModel(&vm);
  menu_null.draw(c_null, 0, 0, 128, 48);
  menu_val.draw(c_val,  0, 0, 128, 48);
  EXPECT_GT(d_val.fills.size(), d_null.fills.size());
}

TEST(Toggle, StateText) {
  EXPECT_STREQ("ON", mishmesh::Toggle::stateText(true));
  EXPECT_STREQ("OFF", mishmesh::Toggle::stateText(false));
}

TEST(Toggle, DrawsPill) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Toggle t;
  t.setOn(true);
  t.draw(c, 100, 0, 28, 12);
  EXPECT_GT(d.fills.size(), 0u);     // pill background/border + text
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
