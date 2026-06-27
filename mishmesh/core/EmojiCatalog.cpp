#include <mishmesh/core/EmojiCatalog.h>

namespace mishmesh {

static const uint32_t* s_catalog = nullptr;
static uint16_t s_catalogCount = 0;

void setEmojiCatalog(const uint32_t* codepoints, uint16_t count) {
  s_catalog = codepoints;
  s_catalogCount = codepoints ? count : 0;
}

uint16_t emojiCatalogCount() { return s_catalogCount; }

uint32_t emojiCatalogAt(uint16_t i) {
  return (s_catalog && i < s_catalogCount) ? s_catalog[i] : 0;
}

}  // namespace mishmesh
