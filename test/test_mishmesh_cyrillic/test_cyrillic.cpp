#include <gtest/gtest.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/text/KeyboardLayouts.h>
#include <mcufont.h>

using namespace mishmesh;

namespace {

// Raw width, not mf_character_width - that one substitutes the fallback glyph's
// width for anything the font lacks, which is exactly what we need to detect.
bool renders(const Font* f, uint16_t cp) {
  return f->character_width(f, (mf_char)cp) != 0;
}

void eachCodepoint(const char* utf8, void (*fn)(uint16_t, void*), void* ctx) {
  if (!utf8) return;
  mf_str p = utf8;
  mf_char ch;
  while ((ch = mf_getchar(&p)) != 0) fn((uint16_t)ch, ctx);
}

// Caption is Tom Thumb 3x6 and ships Latin-1 + Cyrillic only; Latin Extended-A
// is left out on purpose (unreadable at that size, so it draws the block
// placeholder). See regenerate.sh. Asserted below in CaptionOmitsLatinExtendedA.
bool skipFor(const char* fontName, uint16_t cp) {
  return fontName[0] == 'C' && cp >= 0x0100 && cp <= 0x017F;
}

struct Missing {
  const Font* font;
  const char* fontName;
  const char* code;
};

void checkOne(uint16_t cp, void* ctx) {
  Missing* m = (Missing*)ctx;
  if (skipFor(m->fontName, cp)) return;
  if (!renders(m->font, cp)) {
    ADD_FAILURE() << m->fontName << " cannot render U+" << std::hex << cp
                  << " used by layout " << m->code;
  }
}

void sweep(const Font* font, const char* fontName) {
  for (int i = 0; i < kbdLayoutCount(); i++) {
    const KbdLayout& l = kbdLayoutAt(i);
    Missing m{font, fontName, l.code};
    for (int c = 0; c < 9; c++) {
      eachCodepoint(l.lower[c], checkOne, &m);
      eachCodepoint(l.upper[c], checkOne, &m);
      eachCodepoint(l.capsLower[c], checkOne, &m);
      eachCodepoint(l.capsUpper[c], checkOne, &m);
    }
  }
}

}  // namespace

// The invariant that keeps layouts and atlases in step: anything a keyboard can
// produce must have a real glyph, in every font that renders message text.
TEST(CyrillicCoverage, EveryLayoutCodepointRendersInBody) {
  sweep(fontBody(), "Body");
}

TEST(CyrillicCoverage, EveryLayoutCodepointRendersInSubtitle) {
  sweep(fontSubtitle(), "Subtitle");
}

TEST(CyrillicCoverage, EveryLayoutCodepointRendersInCaption) {
  sweep(fontCaption(), "Caption");
}

TEST(CyrillicCoverage, WholeBlockIsPresent) {
  for (uint16_t cp = 0x0400; cp <= 0x045F; cp++) {
    EXPECT_TRUE(renders(fontBody(), cp))     << "Body U+" << std::hex << cp;
    EXPECT_TRUE(renders(fontSubtitle(), cp)) << "Subtitle U+" << std::hex << cp;
    EXPECT_TRUE(renders(fontCaption(), cp))  << "Caption U+" << std::hex << cp;
  }
}

TEST(CyrillicCoverage, GheWithUpturnIsPresent) {
  for (uint16_t cp : {0x0490, 0x0491}) {          // Ukrainian Ґ / ґ
    EXPECT_TRUE(renders(fontBody(), cp));
    EXPECT_TRUE(renders(fontSubtitle(), cp));
    EXPECT_TRUE(renders(fontCaption(), cp));
  }
}

// Stated so the exclusion in skipFor() is a documented contract, not a silent gap.
TEST(CyrillicCoverage, CaptionOmitsLatinExtendedA) {
  EXPECT_FALSE(renders(fontCaption(), 0x0141));   // Ł
  EXPECT_FALSE(renders(fontCaption(), 0x015F));   // ş
  EXPECT_TRUE(renders(fontCaption(), 0x00E9));    // é - Latin-1 is present
}

// Nothing outside the two declared ranges should claim a glyph - uncovered
// codepoints must fall through to Canvas's block placeholder.
TEST(CyrillicCoverage, UncoveredCyrillicStillFallsBack) {
  for (uint16_t cp : {0x0460, 0x0489, 0x048F, 0x0492, 0x04D8, 0x04FF}) {
    EXPECT_FALSE(renders(fontBody(), cp)) << "unexpected glyph at U+" << std::hex << cp;
  }
}

// Letters that were placeholder question marks before the atlases were filled in.
TEST(CyrillicCoverage, FormerlyStubbedLettersHaveDistinctShapes) {
  const Font* f = fontBody();
  // Ѓ (ghe + acute) and Ґ (ghe with upturn) both build on Г and must not collide.
  EXPECT_NE(0, f->character_width(f, 0x0403));
  EXPECT_NE(0, f->character_width(f, 0x0490));
  // Љ and Њ are soft-sign ligatures, so they are wider than a plain letter.
  EXPECT_GT(f->character_width(f, 0x0409), f->character_width(f, 0x041B));  // Љ > Л
  EXPECT_GT(f->character_width(f, 0x040A), f->character_width(f, 0x041D));  // Њ > Н
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
