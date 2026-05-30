#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/widgets/StatusBar.h>
#include <mishmesh/widgets/ListMenu.h>
#include "FakeDisplayDriver.h"

#include <string>
#include <vector>

using namespace mishmesh;

namespace {

bool printed(const FakeDisplayDriver& d, const std::string& s) {
  for (const auto& p : d.prints) if (p == s) return true;
  return false;
}

class VecModel : public ListModel {
public:
  std::vector<std::string> items;
  int count() const override { return (int)items.size(); }
  const char* label(int i) const override { return items[i].c_str(); }
};

}  // namespace

// ---- Canvas anchored text ------------------------------------------------

TEST(CanvasText, WidthForwardsToDriver) {
  FakeDisplayDriver d;
  Canvas c(&d);
  EXPECT_EQ(12, c.textWidth("AB"));   // fake driver = 6px per char
}

TEST(CanvasText, RightAlignsByWidth) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.textRight(100, 0, "AB", DisplayDriver::LIGHT);   // width 12
  EXPECT_EQ(88, d.cursorX);
  EXPECT_TRUE(printed(d, "AB"));
}

TEST(CanvasText, CenteredByHalfWidth) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.textCentered(64, 0, "AB", DisplayDriver::LIGHT);  // width 12
  EXPECT_EQ(58, d.cursorX);
}

TEST(CanvasText, AnchoredTextRespectsRegionOffset) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Canvas sub = c.region(10, 0, 100, 64);
  sub.textRight(50, 0, "AB", DisplayDriver::LIGHT);   // local 50-12=38 -> device 48
  EXPECT_EQ(48, d.cursorX);
}

// ---- Battery ---------------------------------------------------------------

TEST(Battery, MapsVoltageToPercent) {
  EXPECT_EQ(100, batteryPercent(4200));
  EXPECT_EQ(0, batteryPercent(3300));
  EXPECT_EQ(50, batteryPercent(3750));
}

TEST(Battery, ClampsOutOfRange) {
  EXPECT_EQ(100, batteryPercent(5000));
  EXPECT_EQ(0, batteryPercent(3000));
}

// ---- StatusBar -------------------------------------------------------------

TEST(StatusBar, DrawsTitleAndBatteryAsPixels) {
  FakeDisplayDriver d;
  Canvas c(&d);
  StatusBar bar;
  bar.setTitle("node-1");
  bar.setBattery(4200);
  bar.draw(c, 0, 0, 128, 12);
  // mcufont rasterises text as pixel runs: divider + title + battery glyphs.
  EXPECT_GT(d.fills.size(), 1u);
}

// ---- ListMenu --------------------------------------------------------------

TEST(ListMenu, NavDownMovesAndWraps) {
  VecModel m; m.items = {"a", "b", "c"};
  ListMenu list;
  list.setModel(&m);
  EXPECT_EQ(0, list.selected());
  EXPECT_TRUE(list.onInput(InputEvent::NavDown));
  EXPECT_EQ(1, list.selected());
  list.onInput(InputEvent::NavDown);   // -> 2 (last)
  list.onInput(InputEvent::NavDown);   // wraps to the top
  EXPECT_EQ(0, list.selected());
}

TEST(ListMenu, NavUpMovesAndWraps) {
  VecModel m; m.items = {"a", "b", "c"};
  ListMenu list;
  list.setModel(&m);
  EXPECT_TRUE(list.onInput(InputEvent::NavUp));   // wraps from top to bottom
  EXPECT_EQ(2, list.selected());
  list.onInput(InputEvent::NavUp);                // -> 1
  EXPECT_EQ(1, list.selected());
}

TEST(ListMenu, IgnoresNonNavInput) {
  VecModel m; m.items = {"a", "b"};
  ListMenu list;
  list.setModel(&m);
  EXPECT_FALSE(list.onInput(InputEvent::Select));
  EXPECT_FALSE(list.onInput(InputEvent::NavLeft));
  EXPECT_EQ(0, list.selected());
}

TEST(ListMenu, SwitchingModelResetsButRebindingSameKeepsSelection) {
  VecModel a; a.items = {"a", "b", "c"};
  VecModel b; b.items = {"x", "y", "z"};
  ListMenu list;
  list.setModel(&a);
  list.onInput(InputEvent::NavDown);
  EXPECT_EQ(1, list.selected());
  // Re-binding the same model (e.g. returning from a drill-in) keeps position.
  list.setModel(&a);
  EXPECT_EQ(1, list.selected());
  // Switching to a different model resets to the top.
  list.setModel(&b);
  EXPECT_EQ(0, list.selected());
}

TEST(ListMenu, EmptyModelDoesNothing) {
  VecModel m;
  ListMenu list;
  list.setModel(&m);
  EXPECT_FALSE(list.onInput(InputEvent::NavDown));
  EXPECT_EQ(0, list.selected());
}

TEST(ListMenu, HighlightsSelectedRow) {
  VecModel m; m.items = {"a", "b", "c"};
  ListMenu list;
  list.setModel(&m);
  list.setRowHeight(12);
  list.onInput(InputEvent::NavDown);   // select row 1
  FakeDisplayDriver d;
  Canvas c(&d);
  list.draw(c, 0, 0, 128, 60);
  bool found = false;
  for (auto& f : d.fills) if (f.y == 12 && f.h == 12) found = true;
  EXPECT_TRUE(found);
}

TEST(ListMenu, ScrollsToKeepSelectionVisible) {
  VecModel m; m.items = {"a", "b", "c", "d", "e"};
  ListMenu list;
  list.setModel(&m);
  list.setRowHeight(12);
  EXPECT_EQ(0, list.firstVisibleRow(36));         // 3 rows fit, no scroll yet
  for (int i = 0; i < 4; i++) list.onInput(InputEvent::NavDown);  // select "e"
  EXPECT_EQ(2, list.firstVisibleRow(36));         // window shifts to show c,d,e
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
