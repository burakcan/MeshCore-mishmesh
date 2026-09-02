#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Metrics.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/widgets/GridView.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

namespace {

// The four canvases the e-ink builds can actually produce, plus the OLED that
// every fixed pixel constant in the widget set was authored against.
const int OLED_W = 128, OLED_H = 64;      // design target
const int LAND_W = 250, LAND_H = 122;     // e-ink Standard
const int PORT_W = 122, PORT_H = 250;     // e-ink Standard, turned
const int LARGE_W = 125, LARGE_H = 61;    // e-ink Large (landscape)

Canvas canvasOf(FakeDisplayDriver& d) { return Canvas(&d, 0); }

}  // namespace

TEST(Metrics, ShortEdgeDecidesTheTier) {
  FakeDisplayDriver oled(OLED_W, OLED_H);
  FakeDisplayDriver large(LARGE_W, LARGE_H);
  FakeDisplayDriver land(LAND_W, LAND_H);
  FakeDisplayDriver port(PORT_W, PORT_H);

  EXPECT_FALSE(isRegularCanvas(canvasOf(oled)));
  EXPECT_FALSE(isRegularCanvas(canvasOf(large)));
  EXPECT_TRUE(isRegularCanvas(canvasOf(land)));
  // Tall but only 122 wide: the short edge clears the bar, so still Regular.
  EXPECT_TRUE(isRegularCanvas(canvasOf(port)));
}

TEST(Metrics, PortraitIsHeightOverWidth) {
  FakeDisplayDriver land(LAND_W, LAND_H);
  FakeDisplayDriver port(PORT_W, PORT_H);
  EXPECT_FALSE(isPortrait(canvasOf(land)));
  EXPECT_TRUE(isPortrait(canvasOf(port)));
}

TEST(Metrics, CompactCanvasKeepsAuthoredSizes) {
  FakeDisplayDriver oled(OLED_W, OLED_H);
  Canvas c = canvasOf(oled);
  // The 128x64 panel is what the constants were chosen for - nothing shifts.
  EXPECT_EQ(0, rowHeightBonus(c));
  EXPECT_EQ(12, barHeight(c, 12));
}

TEST(Metrics, RegularCanvasOpensRowsAndBars) {
  FakeDisplayDriver land(LAND_W, LAND_H);
  Canvas c = canvasOf(land);
  EXPECT_GT(rowHeightBonus(c), 0);
  EXPECT_GT(barHeight(c, 12), 12);
}

TEST(Canvas, FitScaleClampsToTheWidthItIsGiven) {
  FakeDisplayDriver d(PORT_W, PORT_H);
  Canvas c = canvasOf(d);
  const int w1 = c.textWidth(fontNum(), "00:00.0");
  ASSERT_GT(w1, 0);

  // Exactly enough room for one magnification step and not the next.
  EXPECT_EQ(1, c.fitScale(fontNum(), "00:00.0", w1, 2));
  EXPECT_EQ(1, c.fitScale(fontNum(), "00:00.0", 2 * w1 - 1, 2));
  EXPECT_EQ(2, c.fitScale(fontNum(), "00:00.0", 2 * w1, 2));
  // Never below 1, even when the unscaled string already overflows - the caller
  // ellipsizes rather than being handed a zero.
  EXPECT_EQ(1, c.fitScale(fontNum(), "00:00.0", 1, 2));
  EXPECT_EQ(1, c.fitScale(fontNum(), "00:00.0", 4 * w1, 1));
}

TEST(Canvas, FitScaleToleratesNullAndEmptyInput) {
  FakeDisplayDriver d(OLED_W, OLED_H);
  Canvas c = canvasOf(d);
  EXPECT_EQ(1, c.fitScale(nullptr, "x", 100, 2));
  EXPECT_EQ(1, c.fitScale(fontNum(), nullptr, 100, 2));
  EXPECT_EQ(1, c.fitScale(fontNum(), "0", 0, 2));
  EXPECT_EQ(1, c.fitScale(fontNum(), "0", 100, 0));
}

TEST(Modal, ShrinksToTheHeightTheCallerAsksFor) {
  FakeDisplayDriver d(PORT_W, PORT_H);
  Canvas c = canvasOf(d);
  Canvas box = drawModalChrome(c, 0, 40);
  EXPECT_EQ(40, box.height());
  EXPECT_EQ(PORT_W - 16, box.width());   // no width request: keep the full inset
}

TEST(Modal, NeverGrowsPastTheCanvasInset) {
  FakeDisplayDriver d(OLED_W, OLED_H);
  Canvas c = canvasOf(d);
  // A dialog asking for more than the panel holds is capped, not overflowed.
  Canvas box = drawModalChrome(c, 9999, 9999);
  EXPECT_EQ(OLED_W - 16, box.width());
  EXPECT_EQ(OLED_H - 16, box.height());
}

TEST(Modal, OmittedSizeMatchesTheOldFullInsetBox) {
  FakeDisplayDriver d(OLED_W, OLED_H);
  Canvas c = canvasOf(d);
  Canvas box = drawModalChrome(c);
  EXPECT_EQ(OLED_W - 16, box.width());
  EXPECT_EQ(OLED_H - 16, box.height());
}

namespace {
// 4x4, the shape of the keypad - the densest grid in the system.
struct KeyModel : GridModel {
  int rows() const override { return 4; }
  int cols() const override { return 4; }
  const char* cellLabel(int, int) const override { return "abc"; }
};
}  // namespace

TEST(GridView, CellsAreCappedAndTheBlockIsCentred) {
  KeyModel m;
  GridView g;
  g.setModel(&m);

  FakeDisplayDriver d(PORT_W, PORT_H);
  Canvas c = canvasOf(d);
  d.fills.clear();
  g.setFocus(0, 0);
  g.draw(c, 0, 0, PORT_W, PORT_H);

  // The focus highlight is the only full-cell fill, so it reports the cell size.
  ASSERT_FALSE(d.fills.empty());
  const int maxW = GridView::MAX_CELL_W, maxH = GridView::MAX_CELL_H;
  const auto cell = d.fills.front();
  EXPECT_LE(cell.w, maxW);
  EXPECT_LE(cell.h, maxH);
  // Uncapped this would be 250/4 = 62px tall and start at the very top; capped,
  // the 4-row block sits centred in the panel instead.
  EXPECT_GT(cell.y, 0);
  EXPECT_NEAR((PORT_H - cell.h * 4) / 2, cell.y, 2);
}

TEST(GridView, CompactPanelLayoutIsUnchanged) {
  KeyModel m;
  GridView g;
  g.setModel(&m);

  FakeDisplayDriver d(OLED_W, OLED_H);
  Canvas c = canvasOf(d);
  d.fills.clear();
  g.setFocus(0, 0);
  g.draw(c, 0, 13, OLED_W, OLED_H - 13);

  ASSERT_FALSE(d.fills.empty());
  const auto cell = d.fills.front();
  EXPECT_EQ(OLED_W / 4, cell.w);        // 32px: below the cap, so untouched
  EXPECT_EQ((OLED_H - 13) / 4, cell.h);
  EXPECT_EQ(0, cell.x);
  EXPECT_EQ(13, cell.y);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
