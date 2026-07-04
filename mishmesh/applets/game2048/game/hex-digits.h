#pragma once
#include "defines.h"
// Tile glyphs + invert square. Defined once in hex-digits.cpp — declaring them
// here as file-scope `const` (internal linkage) instead emitted a private copy
// of the 448-byte table into every including TU.
// Sixteen 16x16 bitmaps: 0,1,...,E,F
extern const uint8_t hex_digits[MAX_VALUE + 1][32];
// A white square drawn with "INVERT" will invert the area
extern const uint8_t white_square[32];

// vim:filetype=cpp
