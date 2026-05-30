#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/widgets/StatusBar.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ScrollText.h>
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

// ---- Canvas clipping -------------------------------------------------------

// A nested region at a negative offset (e.g. a list row scrolled partly above
// the body) must not let its draws escape above the parent's clip top - this is
// the bug where toggle pills overlapped the tab bar.
TEST(CanvasClip, NestedNegativeOffsetRegionStaysWithinParent) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Canvas body = c.region(0, 14, 128, 50);     // list body, tabs occupy y 0..13
  Canvas pill = body.region(100, -5, 28, 12); // toggle scrolled 5px above body top
  pill.fillRect(0, 0, 28, 12, DisplayDriver::LIGHT);
  ASSERT_FALSE(d.fills.empty());
  for (const auto& f : d.fills) {
    EXPECT_GE(f.y, 14) << "fill escaped above the parent clip onto the tab bar";
    EXPECT_LE(f.y + f.h, 64);
  }
}

// The same draw must still produce its visible (below-the-fold) portion.
TEST(CanvasClip, NestedNegativeOffsetRegionKeepsVisiblePart) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Canvas body = c.region(0, 14, 128, 50);
  Canvas pill = body.region(100, -5, 28, 12);
  pill.fillRect(0, 0, 28, 12, DisplayDriver::LIGHT);
  ASSERT_EQ(1u, d.fills.size());
  EXPECT_EQ(14, d.fills[0].y);     // clipped to the body top
  EXPECT_EQ(7, d.fills[0].h);      // 12 - 5 px clipped away
  EXPECT_EQ(100, d.fills[0].x);
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

TEST(ListMenu, EmptyModelDrawsNoSelectionBar) {
  VecModel m;   // no rows
  ListMenu list; list.setModel(&m); list.setRowHeight(12);
  list.setEmptyText("No items");
  FakeDisplayDriver d; Canvas c(&d);
  list.draw(c, 0, 0, 128, 60);
  for (auto& f : d.fills) EXPECT_NE(12, f.h);   // no row-height highlight bar
  EXPECT_FALSE(list.needsAnimation());
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

TEST(ListMenu, AnimatesSelectionThenSettlesOnRow) {
  VecModel m; m.items = {"a", "b", "c", "d"};
  ListMenu list; list.setModel(&m); list.setRowHeight(12);
  FakeDisplayDriver d; Canvas c(&d);
  list.draw(c, 0, 0, 128, 60);            // first draw snaps to the top
  EXPECT_FALSE(list.needsAnimation());

  list.onInput(InputEvent::NavDown);       // -> row 1: should glide, not jump
  list.draw(c, 0, 0, 128, 60);
  EXPECT_TRUE(list.needsAnimation());      // mid-slide

  for (int i = 0; i < 30 && list.needsAnimation(); i++) list.draw(c, 0, 0, 128, 60);
  EXPECT_FALSE(list.needsAnimation());     // settled

  d.fills.clear();
  list.draw(c, 0, 0, 128, 60);
  bool barAtRow1 = false;
  for (auto& f : d.fills) if (f.y == 12 && f.h == 12) barAtRow1 = true;
  EXPECT_TRUE(barAtRow1);
}

TEST(ListMenu, WrappingSnapsInsteadOfSlidingAcrossList) {
  VecModel m; m.items = {"a", "b", "c"};
  ListMenu list; list.setModel(&m); list.setRowHeight(12);
  FakeDisplayDriver d; Canvas c(&d);
  list.draw(c, 0, 0, 128, 60);
  list.onInput(InputEvent::NavUp);         // wrap 0 -> 2
  list.draw(c, 0, 0, 128, 60);
  EXPECT_EQ(2, list.selected());
  EXPECT_FALSE(list.needsAnimation());     // snapped, no long slide
}

TEST(ScrollText, AnimatesScrollThenSettles) {
  ScrollText st;
  for (int i = 0; i < 20; i++) st.addf("line %d", i);   // overflows a short view
  FakeDisplayDriver d; Canvas c(&d);
  st.draw(c, 0, 0, 128, 40);            // first draw snaps to the top
  EXPECT_FALSE(st.needsAnimation());

  st.onInput(InputEvent::NavDown);       // scroll down a line: should glide
  st.draw(c, 0, 0, 128, 40);
  EXPECT_TRUE(st.needsAnimation());

  for (int i = 0; i < 30 && st.needsAnimation(); i++) st.draw(c, 0, 0, 128, 40);
  EXPECT_FALSE(st.needsAnimation());     // settled
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
