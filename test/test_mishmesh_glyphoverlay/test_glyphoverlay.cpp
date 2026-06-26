// Tests the Canvas glyph-overlay hook (Canvas::setEmojiRenderer + mm_char +
// overlay-aware textWidth) using the committed iconFont() as a stand-in overlay,
// so no licensed emoji art is needed. The real emoji atlas is a local-only,
// gitignored overlay that registers into this same hook on device.
#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <mcufont.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

// Stand-in overlay: map '@' (0x40) to the Home icon glyph (0xE000); treat VS16 as
// zero-width. (Real device lookups come from the gitignored emoji module.)
static bool ovLookup(uint16_t key, uint16_t& glyph) {
  if (key == 0x40) { glyph = 0xE000; return true; }
  return false;
}
static bool ovZeroWidth(uint16_t key) { return key == 0xFE0F; }

struct GlyphOverlay : ::testing::Test {
  void TearDown() override { Canvas::setEmojiRenderer(nullptr, nullptr, nullptr); }
};

// With no overlay registered, measuring is byte-identical to upstream.
TEST_F(GlyphOverlay, UnregisteredMeasuresStock) {
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  EXPECT_EQ(fontBody()->character_width(fontBody(), 'A'), c.textWidth(fontBody(), "A"));
}

// A mapped codepoint measures at the OVERLAY glyph's advance, not the body glyph.
TEST_F(GlyphOverlay, MappedKeyMeasuresAtOverlayAdvance) {
  Canvas::setEmojiRenderer(iconFont(), ovLookup, ovZeroWidth);
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  int overlayAdv = iconFont()->character_width(iconFont(), 0xE000);
  EXPECT_GT(overlayAdv, 0);
  // Overlay glyphs get 1px side bearing each side (kEmojiPadPx) so they don't
  // butt against neighbours -> measured width is the glyph advance + 2.
  EXPECT_EQ(overlayAdv + 2, c.textWidth(fontBody(), "@"));
}

// A mapped codepoint renders the overlay glyph's pixels (Home icon is non-empty).
TEST_F(GlyphOverlay, MappedKeyRendersOverlayGlyph) {
  Canvas::setEmojiRenderer(iconFont(), ovLookup, ovZeroWidth);
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  c.drawText(fontBody(), 0, 0, "@", DisplayDriver::LIGHT);
  EXPECT_GT(d.fills.size(), 0u);
}

// Zero-width modifiers measure 0 and draw nothing.
TEST_F(GlyphOverlay, ZeroWidthMeasuresZeroAndDrawsNothing) {
  Canvas::setEmojiRenderer(iconFont(), ovLookup, ovZeroWidth);
  FakeDisplayDriver d(128, 64);
  Canvas c(&d);
  EXPECT_EQ(0, c.textWidth(fontBody(), "\xEF\xB8\x8F"));   // U+FE0F alone
  c.drawText(fontBody(), 0, 0, "\xEF\xB8\x8F", DisplayDriver::LIGHT);
  EXPECT_EQ(0u, d.fills.size());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
