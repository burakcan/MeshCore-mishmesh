#pragma once
#include <stdint.h>
#include "defines.h"
#include "globals.h"
#include "stars.h"

// Mapped Arduboy functions
class Text {
public:
  static void DrawString(const uint8_t* text, uint8_t x, uint8_t y,
      uint8_t colour=COLOUR_WHITE);
  static void DrawInt(uint16_t value, uint8_t x, uint8_t y,
      uint8_t colour=COLOUR_WHITE);
};

// Game functions
void InitScreen();
void DrawLabels();
void EraseRect(int16_t x, int16_t y, uint8_t w, uint8_t h);
void DrawMap(uint16_t board[DIM][DIM]);
void Flash(uint16_t board[DIM][DIM]);
void DrawGameState(bool running);
void DrawReward(uint16_t value, Rectangle* rect);
void DrawScore(uint16_t score, uint16_t highscore, uint16_t biggest);
void DrawGameOver();
// vim: tabstop=2:softtabstop=2:shiftwidth=2:expandtab:filetype=cpp
