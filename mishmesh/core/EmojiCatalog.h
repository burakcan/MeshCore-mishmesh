#pragma once

#include <stdint.h>

namespace mishmesh {

// Registry of pickable emoji codepoints (full Unicode, e.g. 0x1F600). Registered
// once by the local emoji overlay; empty otherwise, which leaves the keypad's
// emoji picker dormant. No art here, just codepoints supplied at registration.
void     setEmojiCatalog(const uint32_t* codepoints, uint16_t count);
uint16_t emojiCatalogCount();
uint32_t emojiCatalogAt(uint16_t i);   // 0 if i is out of range

}  // namespace mishmesh
