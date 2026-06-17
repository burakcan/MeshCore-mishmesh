// Compact font taken from
// https://hackaday.io/project/6309-vga-graphics-over-spi-and-serial-vgatonic/log/20759-a-tiny-4x6-pixel-font-that-will-fit-on-almost-any-microcontroller-license-mit

#include <stdint.h>
#include "defines.h"
#include "font.h"
#include "platform.h"

uint16_t Font::CharIdx(const uint8_t c) {

  uint16_t idx;

  switch (c) {

    case CHAR_LETTER_A ... CHAR_LETTER_Z:
      idx = c - CHAR_LETTER_A;
      break;

    case CHAR_LETTER_A_LOWER ... CHAR_LETTER_Z_LOWER:
      idx = c - CHAR_LETTER_A_LOWER + 26;
      break;

    case CHAR_NUMBER_0 ... CHAR_NUMBER_9:
      idx = c - CHAR_NUMBER_0 + FONT_NUMBER_INDEX;
      break;

    case '!':
      idx = FONT_EXCLAMATION_INDEX;
      break;

    case '.':
      idx = FONT_PERIOD_INDEX;
      break;

    case ' ':
      idx = FONT_SPACE_INDEX;
      break;

    case 0x27:
      idx = FONT_APOSTROPHE_INDEX;
      break;

    case 0xA9:
      idx = FONT_EACUTE_INDEX;
      break;

    case 0xAA:
      idx = FONT_ECIRCONFLEX_INDEX;
      break;

    case 0x3A:
      idx = FONT_COLON_INDEX;
      break;

    default:
      idx = FONT_BIDON_INDEX;
  }

  return idx * glyphWidth;
}

void Font::PrintString(const uint8_t* str, uint8_t x, uint8_t y,
    uint8_t colour) {
  Coord pos;

  pos.x = x;
  pos.y = y;

	for (;;)
	{
		uint8_t c = *str++;
		if (!c)
			break;
    if (c == 0xC3)
      c = *str++;

    // We are cheating with type conversion here in order
    // to remain compatible with the Arduboy version
		DrawChar(&pos, c, colour);
    if (pos.x > DISPLAY_WIDTH - glyphWidth - 1) {
      pos.y += FONT_HEIGHT;
    }
    pos.x += FONT_WIDTH; // uint8_t. Wraps around automatically.
	}
}

void Font::PrintInt(uint16_t val, uint8_t x, uint8_t y, uint8_t colour)
{
  Coord pos;

  pos.x = x;
  pos.y = y;

	if (val == 0)
	{
		DrawChar(&pos, '0', colour);
		return;
	}

	constexpr int maxDigits = 5;
	char buffer[maxDigits];
	int bufCount = 0;

	for (int n = 0; n < maxDigits && val != 0; n++)
	{
		unsigned char c = val % 10;
		buffer[bufCount++] = '0' + c;
		val = val / 10;
	}

	for (int n = bufCount - 1; n >= 0; n--)
	{
		DrawChar(&pos, buffer[n], colour);
    if (pos.x > DISPLAY_WIDTH - glyphWidth - 1) {
      pos.y += FONT_HEIGHT;
    }
    pos.x += FONT_WIDTH;
	}
}

// From font.cpp
void Font::DrawChar(Coord* pos, uint8_t c, uint8_t colour) {

  uint16_t idx = CharIdx(c);
  const uint8_t* fontPtr = font_images + idx;

  Platform::DrawBitmap(fontPtr, pos->x, pos->y, glyphWidth, glyphHeight);
}

// vim:ft=cpp:fdm=syntax

