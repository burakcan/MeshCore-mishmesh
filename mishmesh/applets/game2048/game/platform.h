#pragma once

#include <stdint.h>
#include "defines.h"
#include "globals.h"

class Platform
{
public:
  // General
	static uint8_t ButtonState();
  static uint8_t* GetBuffer();
  static int16_t Random(int16_t i0, int16_t i1);
  static uint32_t GenerateRandomSeed();
  static void InitRandomSeed();
  // Drawing
  static void DrawBuffer(); // Do we need this?
  static void PutPixel(uint8_t x, uint8_t y, uint8_t colour=COLOUR_WHITE);
  static void DrawBitmap(const uint8_t* bitmap, int16_t x, int16_t y,
                  uint8_t w, uint8_t h, uint8_t colour=COLOUR_WHITE);
  static void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
      uint8_t colour=COLOUR_WHITE);
  static void DrawRect(int16_t x, int16_t y, uint8_t w, uint8_t h,
      uint8_t colour=COLOUR_WHITE);
  static void DrawFilledRect(int16_t x, int16_t y, uint8_t w, uint8_t h,
      uint8_t colour=COLOUR_WHITE);
  static void DrawCircle(int16_t x0, int16_t y0, uint8_t r,
      uint8_t colour=COLOUR_WHITE);
  static void DrawFilledCircle(int16_t x0, int16_t y0, uint8_t r,
      uint8_t colour=COLOUR_WHITE);
  static void FillScreen(uint8_t colour);
  static void Clear();
  // Timer
  static uint32_t Millis();

  // EEPROM
  static SavedState ToEEPROM(const uint8_t *bytes, const uint16_t offset,
      const uint16_t sz);
  static SavedState FromEEPROM(uint8_t *bytes, const uint16_t offset,
      const uint16_t length);
	/* static void PlaySound(const uint16_t* audioPattern); */
	/* static bool IsAudioEnabled(); */
	/* static void SetAudioEnabled(bool isEnabled); */

	/* static void ExpectLoadDelay(); */

  // Debug
#ifdef DEV_DEB
  static void DebugPrint(uint16_t value, bool endl=false);
  static void DebugPrint(int16_t value, bool endl=false);
  static void DebugPrint(float value, bool endl=false);
  static void DebugPrint(const uint8_t* text, bool endl=false);
#endif
	
};
// vim:ft=cpp
