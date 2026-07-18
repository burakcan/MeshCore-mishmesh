// Registers the EmojiMania overlay atlas into
// the generic Canvas glyph-overlay hook. This is the committed source template;
// the generator copies it into the gitignored emoji-local/, where it compiles
// next to the buyer-generated atlas (Emoji.c) and map (EmojiMap.inc, included
// below). Without the sheet, emoji-local/ is empty on a fresh clone, the overlay
// never registers, and the build is identical to upstream. The atlas is the art
// and is never committed.
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/EmojiCatalog.h>
#include <mf_bwfont.h>

extern "C" {
extern const struct mf_bwfont_s mf_bwfont_Emoji;   // generated: Emoji.c (this dir)
}

namespace {

#include "EmojiMap.inc"   // kEmojiMap[], kEmojiZeroWidth[]  (generated)

bool emojiLookup(uint16_t key, uint16_t& glyphOut) {
  for (const EmojiEntry& e : kEmojiMap) {
    if (e.key == key) { glyphOut = e.glyph; return true; }
  }
  return false;
}

bool emojiZeroWidth(uint16_t key) {
  for (uint16_t z : kEmojiZeroWidth) {
    if (z == key) return true;
  }
  return false;
}

// Self-register at static-init (pointer stores only -- safe before main, no heap;
// mf_bwfont_Emoji is const-initialized so its address is valid here). This is the
// only wiring: no committed file names this overlay.
const bool _emojiRegistered = (
    mishmesh::Canvas::setEmojiRenderer(
        (const mf_font_s*)&mf_bwfont_Emoji, &emojiLookup, &emojiZeroWidth),
    mishmesh::setEmojiCatalog(kEmojiCatalog,
        (uint16_t)(sizeof(kEmojiCatalog) / sizeof(kEmojiCatalog[0]))),
    true);

}  // namespace
