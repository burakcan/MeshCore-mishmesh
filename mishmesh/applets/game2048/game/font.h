#pragma once

#include <stdint.h>
#include "defines.h"
#include "font4x6.h"

#define FONT_WIDTH FONT4x6_WIDTH
#define FONT_HEIGHT FONT4x6_HEIGHT

typedef struct {
  uint8_t x;
  uint8_t y;
} Coord;

class Font
{
public:
	static constexpr uint8_t glyphWidth = GLYPH4x6_WIDTH;
  static constexpr uint8_t glyphHeight = GLYPH4x6_HEIGHT;

	void PrintString(const uint8_t* str, uint8_t x,
      uint8_t y, uint8_t colour = COLOUR_WHITE);
	void PrintInt(uint16_t value, uint8_t x,
      uint8_t y, uint8_t colour = COLOUR_WHITE);

private:
  uint16_t CharIdx(const uint8_t c);
	void DrawChar(Coord* pos, uint8_t c, uint8_t colour);
};
// vim:ft=cpp
