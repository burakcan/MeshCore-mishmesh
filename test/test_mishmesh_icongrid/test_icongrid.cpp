#include <gtest/gtest.h>
#include <mishmesh/widgets/IconGrid.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
#include <stdio.h>
using namespace mishmesh;

namespace {
// Flat drawer model: `n` items, every item has an icon, item 0 may carry a badge.
struct DrawerModel : ListModel {
  int n = 7;
  const char* badge = nullptr;
  int count() const override { return n; }
  const char* label(int i) const override {
    static char buf[8];
    snprintf(buf, sizeof(buf), "app%d", i);
    return buf;
  }
  uint16_t icon(int) const override { return (uint16_t)Icon::Home; }
  const char* value(int i) const override { return i == 0 ? badge : nullptr; }
};

bool hasFill(const FakeDisplayDriver& d, int x, int y, int w, int h) {
  for (const auto& f : d.fills)
    if (f.x == x && f.y == y && f.w == w && f.h == h) return true;
  return false;
}
}

TEST(IconGrid, NavMovesByOneWithWrap) {
  DrawerModel m;                       // 7 items, single-column list
  IconGrid g;
  g.setModel(&m);
  EXPECT_EQ(0, g.selected());
  EXPECT_TRUE(g.onInput(InputEvent::NavDown));   // 0 -> 1
  EXPECT_EQ(1, g.selected());
  EXPECT_TRUE(g.onInput(InputEvent::NavUp));     // 1 -> 0
  EXPECT_EQ(0, g.selected());
  EXPECT_TRUE(g.onInput(InputEvent::NavUp));     // 0 -> 6 (wrap up)
  EXPECT_EQ(6, g.selected());
  EXPECT_TRUE(g.onInput(InputEvent::NavDown));   // 6 -> 0 (wrap down)
  EXPECT_EQ(0, g.selected());
  EXPECT_TRUE(g.onInput(InputEvent::NavRight));  // right behaves like down
  EXPECT_EQ(1, g.selected());
  EXPECT_TRUE(g.onInput(InputEvent::NavLeft));   // left behaves like up
  EXPECT_EQ(0, g.selected());
}

TEST(IconGrid, SelectAndBackBubble) {
  DrawerModel m;
  IconGrid g;
  g.setModel(&m);
  EXPECT_FALSE(g.onInput(InputEvent::Select));
  EXPECT_FALSE(g.onInput(InputEvent::Back));
}

TEST(IconGrid, NoModelDoesNotCrashOrConsume) {
  IconGrid g;
  EXPECT_FALSE(g.onInput(InputEvent::NavRight));
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);
  EXPECT_EQ(0u, d.fills.size());
}

// List geometry: 1 col, 20px rows, 16px icon box. Font metrics are read from the
// real fonts so the tests don't hardcode atlas sizes. 7 items -> contentH 140 >
// 64, so there's a scrollbar and cw = 124.
TEST(IconGrid, FocusedRowFillsRoundedHighlight) {
  DrawerModel m;
  m.n = 7;
  IconGrid g;
  g.setModel(&m);
  g.setRowHeight(20);
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);
  // focused row 0: fillRoundRect(1, 1, 122, 18) emits center block (2, 1, 120, 18).
  EXPECT_TRUE(hasFill(d, 2, 1, 120, 18));
  EXPECT_FALSE(g.needsAnimation());     // first draw snaps
}

TEST(IconGrid, RowDrawsIconBox) {
  DrawerModel m;
  m.n = 7;
  IconGrid g;
  g.setModel(&m);
  g.setRowHeight(20);
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);
  // row 1 (unfocused) icon box drawRoundRect(2, boxY, 16, 16) -> top edge (3, boxY, 14, 1).
  int boxY = 20 + (20 - 16) / 2;        // row 1 top = 20, box centered -> 22
  EXPECT_TRUE(hasFill(d, 3, boxY, 14, 1));
}

TEST(IconGrid, ScrollSnapsToKeepFocusedRowVisibleAndDrawsScrollbar) {
  DrawerModel m;
  m.n = 7;
  IconGrid g;
  g.setModel(&m);
  g.setRowHeight(20);
  for (int i = 0; i < 4; i++) g.onInput(InputEvent::NavDown);   // -> row 4
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);
  // selTop = 4*20 = 80; scrollTarget = 80+20-64 = 36; snap. Row 4 at ty = 80-36 = 44.
  // focused fillRoundRect(1, 45, 122, 18) -> center block (2, 45, 120, 18).
  EXPECT_TRUE(hasFill(d, 2, 45, 120, 18));
  bool thumb = false;
  for (const auto& f : d.fills) if (f.x == 126 && f.w == 2) thumb = true;
  EXPECT_TRUE(thumb);
}

TEST(IconGrid, ScrollEasesAfterFirstDraw) {
  DrawerModel m;
  m.n = 7;
  IconGrid g;
  g.setModel(&m);
  g.setRowHeight(20);
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);             // snap at scroll 0
  for (int i = 0; i < 4; i++) g.onInput(InputEvent::NavDown);   // target becomes 36
  FakeDisplayDriver d2;
  Canvas c2(&d2);
  g.draw(c2, 0, 0, 128, 64);            // eased step (rowH/2 = 10 < 36)
  EXPECT_TRUE(g.needsAnimation());
  for (int i = 0; i < 12; i++) { FakeDisplayDriver dx; Canvas cx(&dx); g.draw(cx, 0, 0, 128, 64); }
  EXPECT_FALSE(g.needsAnimation());     // settled
}

TEST(IconGrid, BadgeBubbleDrawnAtRowRight) {
  DrawerModel m;
  m.n = 7;
  m.badge = "3";
  IconGrid g;
  g.setModel(&m);
  g.setRowHeight(20);
  g.onInput(InputEvent::NavDown);       // focus row 1; row 0 (badged) unfocused
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);
  int cw = 124;                         // 7 rows -> scrollbar
  int bw = c.textWidth(fontCaption(), "3") + 3;
  int bh = c.fontHeight(fontCaption()) + 2;
  int bx = cw - 2 - bw;                 // MARGIN=2 from the right edge of the row
  int by = (20 - bh) / 2;
  EXPECT_TRUE(hasFill(d, bx, by, bw, bh));
}

TEST(IconGrid, FocusedOverflowingLabelMarquees) {
  struct LongModel : ListModel {
    int count() const override { return 2; }
    const char* label(int i) const override { return i == 0 ? "Supercalifragilisticexpialidocious" : "b"; }
    uint16_t icon(int) const override { return (uint16_t)Icon::Home; }
  } lm;
  IconGrid g;
  g.setModel(&lm);
  g.setRowHeight(20);                   // 2 rows, no scrollbar; row 0 label overflows
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);             // row 0 focused + overflowing -> marquee active
  EXPECT_TRUE(g.needsAnimation());
}

// Regression: navigating from an overflowing (marqueeing) row to a fitting one
// must stop the animation once the highlight settles. Guards Marquee::reset()
// clearing _active - otherwise needsAnimation() sticks true and the menu
// redraws at 30fps forever even after the glide finishes.
TEST(IconGrid, NavAwayFromOverflowingLabelStopsAnimating) {
  struct LongModel : ListModel {
    int count() const override { return 2; }
    const char* label(int i) const override { return i == 0 ? "Supercalifragilisticexpialidocious" : "b"; }
    uint16_t icon(int) const override { return (uint16_t)Icon::Home; }
  } lm;
  IconGrid g;
  g.setModel(&lm);
  g.setRowHeight(20);
  FakeDisplayDriver d;
  Canvas c(&d);
  g.draw(c, 0, 0, 128, 64);             // row 0 marquees
  EXPECT_TRUE(g.needsAnimation());
  g.onInput(InputEvent::NavDown);       // focus row 1 ("b", fits)
  // Let the highlight glide settle; after that the only thing that could keep
  // needsAnimation() true is a stale marquee _active flag.
  for (int i = 0; i < 12; i++) { FakeDisplayDriver dx; Canvas cx(&dx); g.draw(cx, 0, 0, 128, 64); }
  EXPECT_FALSE(g.needsAnimation());     // highlight settled AND marquee deactivated
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
