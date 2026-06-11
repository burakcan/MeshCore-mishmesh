#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <mcufont.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

TEST(Fonts, AccessorsResolve) {
  EXPECT_NE(nullptr, fontBody());
  EXPECT_NE(nullptr, fontSubtitle());
  EXPECT_NE(nullptr, fontNum());
  EXPECT_NE(nullptr, iconFont());
}

TEST(Text, WidthIsPositiveAndGrowsWithLength) {
  FakeDisplayDriver d;
  Canvas c(&d);
  int hi = c.textWidth(fontBody(), "Hi");
  int hello = c.textWidth(fontBody(), "Hello world");
  EXPECT_GT(hi, 0);
  EXPECT_GT(hello, hi);
}

TEST(Text, DrawEmitsPixels) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.drawText(fontBody(), 0, 0, "Hi", DisplayDriver::LIGHT);
  EXPECT_FALSE(d.fills.empty());   // glyphs drawn as pixel runs
}

TEST(Text, EmptyStringDrawsNothing) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.drawText(fontBody(), 0, 0, "", DisplayDriver::LIGHT);
  EXPECT_TRUE(d.fills.empty());
}

TEST(Text, WrappingProducesMultipleLines) {
  FakeDisplayDriver d;
  Canvas c(&d);
  const char* para = "The quick brown fox jumps over the lazy dog again and again";
  int lh = c.lineHeight(fontBody());
  ASSERT_GT(lh, 0);
  int endY = c.drawTextWrapped(fontBody(), 0, 0, 80, para, DisplayDriver::LIGHT);
  EXPECT_GE(endY, lh * 2);   // wrapped onto at least two lines
}

TEST(Text, GlyphRendersAnIcon) {
  FakeDisplayDriver d;
  Canvas c(&d);
  c.drawGlyph(iconFont(), 0, 0, (uint16_t)Icon::Home, DisplayDriver::LIGHT);
  EXPECT_FALSE(d.fills.empty());
}

TEST(Fonts, CommonEULatinCovered) {
  // Synthesized Latin Extended-A (Common EU Latin) must render in the text fonts.
  // character_width here is the raw per-font lookup (no fallback), so >0 means a
  // real glyph exists rather than the '?'/block placeholder.
  const mf_font_s* fonts[] = { fontBody(), fontSubtitle() };
  const uint16_t present[] = {
    0x105,0x107,0x119,0x142,0x144,0x15B,0x17C,   // Polish
    0x10D,0x11B,0x148,0x159,0x161,0x17E,0x165,   // Czech/Slovak
    0x151,0x171,                                 // Hungarian
    0x111,                                       // Croatian
    0x103,0x15F,0x163,                           // Romanian
    0x11F,0x130,0x131,                           // Turkish
  };
  for (const mf_font_s* f : fonts)
    for (uint16_t cp : present)
      EXPECT_GT(f->character_width(f, cp), 0) << "missing glyph U+" << std::hex << cp;
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
