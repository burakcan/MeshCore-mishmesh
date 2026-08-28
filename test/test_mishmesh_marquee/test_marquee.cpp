#include <gtest/gtest.h>
#include <mishmesh/widgets/Marquee.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
#include <vector>

using namespace mishmesh;

namespace {

const int AVAIL = 60;   // narrow enough that the label below overflows it
const int ROW_H = 14;
const char* LONG_TEXT = "a name far too long to fit in a single row";

struct Shot {
  FakeDisplayDriver d;
  Marquee m;
  // Draws one frame at `now` and returns the ink pattern, so two frames can be
  // compared without knowing anything about glyph shapes.
  std::vector<FakeDisplayDriver::Rect> frame(uint32_t now, const char* text = LONG_TEXT) {
    d.fills.clear();
    Canvas c(&d, now);
    m.draw(c, fontBody(), 0, 0, AVAIL, ROW_H, text, DisplayDriver::LIGHT, now);
    return d.fills;
  }
};

bool sameInk(const std::vector<FakeDisplayDriver::Rect>& a,
             const std::vector<FakeDisplayDriver::Rect>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].w != b[i].w || a[i].h != b[i].h) return false;
  }
  return true;
}

struct ReducedMotion {   // the flag is global; never leak it into another test
  ReducedMotion() { setReducedMotion(true); }
  ~ReducedMotion() { setReducedMotion(false); }
};

}  // namespace

TEST(MarqueeReducedMotion, TextThatFitsIsNeverAnimated) {
  ReducedMotion rm;
  Shot s;
  s.frame(0, "short");
  EXPECT_FALSE(s.m.active());
}

TEST(MarqueeReducedMotion, StepsToANewWindowOnTheNextPage) {
  ReducedMotion rm;
  Shot s;
  auto page0 = s.frame(1000);
  EXPECT_TRUE(s.m.active());        // there is more to show
  auto still0 = s.frame(1000 + Marquee::WRAP_MS - 1);   // inside the first dwell
  EXPECT_TRUE(sameInk(page0, still0));

  auto page1 = s.frame(1000 + Marquee::WRAP_MS);
  EXPECT_FALSE(sameInk(page0, page1));   // the window moved along
  EXPECT_TRUE(s.m.active());
}

TEST(MarqueeReducedMotion, TheStartIsHeldForLessThanTheOtherPages) {
  ReducedMotion rm;
  Shot s;
  s.frame(1000);   // the first draw anchors the phase, so start there
  auto page1 = s.frame(1000 + Marquee::WRAP_MS);
  auto still1 = s.frame(1000 + Marquee::WRAP_MS + Marquee::PAGE_MS - 1);
  EXPECT_TRUE(sameInk(page1, still1));   // a normal page gets the full dwell
  auto page2 = s.frame(1000 + Marquee::WRAP_MS + Marquee::PAGE_MS);
  EXPECT_FALSE(sameInk(page1, page2));
  EXPECT_LT(uint32_t(Marquee::WRAP_MS), uint32_t(Marquee::PAGE_MS));
}

TEST(MarqueeReducedMotion, KeepsLoopingInsteadOfSettling) {
  ReducedMotion rm;
  Shot s;
  auto page0 = s.frame(1000);

  int pages = 1;   // mirror the widget's paging so the cycle length is predictable
  {
    FakeDisplayDriver probe;
    Canvas c(&probe, 0);
    const int step = AVAIL - 8;
    while ((pages - 1) * step + AVAIL < c.textWidth(fontBody(), LONG_TEXT)) pages++;
  }
  ASSERT_GT(pages, 1);
  const uint32_t cycle = Marquee::WRAP_MS + (uint32_t)(pages - 1) * Marquee::PAGE_MS;

  for (uint32_t t = 1000; t <= 1000 + cycle * 4; t += 300) s.frame(t);
  EXPECT_TRUE(s.m.active());                 // still going, several passes in

  auto wrapped = s.frame(1000 + cycle * 5);  // exactly on a cycle boundary
  EXPECT_TRUE(sameInk(page0, wrapped));      // back at the start, not stuck
}

TEST(MarqueeReducedMotion, ResetStartsThePhaseOver) {
  ReducedMotion rm;
  Shot s;
  auto page0 = s.frame(1000);
  auto later = s.frame(1000 + Marquee::WRAP_MS + Marquee::PAGE_MS);
  ASSERT_FALSE(sameInk(page0, later));       // mid-cycle
  s.m.reset();
  auto restarted = s.frame(1000 + Marquee::WRAP_MS + Marquee::PAGE_MS);
  EXPECT_TRUE(sameInk(page0, restarted));    // a new selection begins at the start
}

TEST(MarqueeReducedMotion, ASlowFlushDoesNotLookLikeAFreshEntry) {
  ReducedMotion rm;
  Shot s;
  auto page0 = s.frame(1000);
  // Frames arrive seconds apart on e-ink. With the fast-panel re-entry window
  // (300ms) every frame would restart the phase and page 0 would never advance.
  auto page1 = s.frame(1000 + 2500);
  EXPECT_FALSE(sameInk(page0, page1));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
