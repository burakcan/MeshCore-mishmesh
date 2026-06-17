/* Helper functions to unclutter main file
 */
#include "defines.h"
#include "draw.h"
#include "platform.h"
#include "hex-digits.h"
#include "rewards.h"
#include "utils.h"
#include "font.h"

#define ERASE_BOARD() EraseRect(BOARD_X, BOARD_Y, TILE_SZ * DIM, TILE_SZ * DIM)

Font font;

void Text::DrawString(const uint8_t* text, uint8_t x, uint8_t y,
                      uint8_t colour) {
  font.PrintString(U8(text), x, y, colour);
}

void Text::DrawInt(uint16_t value, uint8_t x, uint8_t y, uint8_t colour) {
  font.PrintInt(value, x, y, colour);
}

void InitScreen() {

  Platform::Clear();

  Text::DrawString(U8"Score", SCORE_X, SCORE_LINE * FONT_STEP);
  Text::DrawString(U8"HiSco", SCORE_X, HIGHSCORE_LINE * FONT_STEP);
  Text::DrawString(U8"Max", SCORE_X, MAX_LINE * FONT_STEP);
}

void EraseRect(int16_t x, int16_t y, uint8_t w, uint8_t h) {
  Platform::DrawFilledRect(x, y, w, h, COLOUR_BLACK);
}

void DrawMap(uint16_t board[DIM][DIM]) {
  int16_t i, value;
  // Draw 4x4 map of numbers in board.
  ERASE_BOARD();

  for (i = 0; i < PLACES; i++) {
    if ((value = board[i >> 2][i & 3])) {
      value &= 0x7FFF;
      Platform::DrawBitmap(hex_digits[value], 32 + ((i << 2) & 0x30),
          (i << 4) & 0x30, 16, 16, COLOUR_WHITE);
    }
  }
}

void Flash(uint16_t board[DIM][DIM]) {
  // Flash Squares with highest bit set
  uint16_t value;
  int i;
  for (i = 0; i < PLACES; i++) {
    value = board[i >> 2][i & 3];
    if (value & 0x8000 ) {
      //*value &= 0x7FFF;
      Platform::DrawBitmap(white_square, 32 + ((i << 2) & 0x30), (i << 4) & 0x30,
                 16, 16, COLOUR_INVERT);
    }
  }
}

void DrawGameState(bool /*running*/) {
  // [mishmesh] Upstream drew a running/game-over debug dot at (14,30) "for testing";
  // removed — it served no gameplay purpose. Kept as a no-op so the call site is intact.
}

void DrawScore(uint16_t score, uint16_t highscore, uint16_t biggest) {
  EraseRect(SCORE_X, (SCORE_LINE + 1) * FONT_STEP,
      DISPLAY_WIDTH - SCORE_X, FONT_STEP);
  Text::DrawInt(score, SCORE_X, FONT_STEP * (SCORE_LINE + 1));

  EraseRect(SCORE_X, (HIGHSCORE_LINE + 1) * FONT_STEP,
                      DISPLAY_WIDTH - SCORE_X, FONT_STEP);
  Text::DrawInt(highscore, SCORE_X, FONT_STEP * (HIGHSCORE_LINE + 1));

  EraseRect(SCORE_X, (MAX_LINE + 1) * FONT_STEP,
                      DISPLAY_WIDTH - SCORE_X, FONT_STEP);
  Text::DrawInt(biggest, SCORE_X, FONT_STEP * (MAX_LINE + 1));

}

void DrawGameOver() {
  uint8_t gameover_sz = len(U8(gameoverstr)) * FONT_WIDTH;
  uint8_t gameover_x = (DISPLAY_WIDTH - gameover_sz) / 2;
  uint8_t rect_x = gameover_x - 4;
  uint8_t rect_w = gameover_sz + 8;

  // EraseRect area
  EraseRect(rect_x, 8 * OVER_LINE - 4, rect_w, FONT_HEIGHT + 8);
  // Draw double border
  Platform::DrawRect(rect_x, 8 * OVER_LINE - 4, rect_w,
                     FONT_HEIGHT + 8);
  Platform::DrawRect(rect_x - 1, 8 * OVER_LINE - 5, rect_w + 2,
                     FONT_HEIGHT + 10);
  // Write "Game Over"
  Text::DrawString(U8(gameoverstr), gameover_x,
                   OVER_LINE * FONT_HEIGHT + 1);
}

void DrawReward(uint16_t value, Rectangle* rect) {
  uint16_t bit_offset;
  uint16_t bit_width = sizeof(reward) / 2;

  switch (value) {
    case 2048:
      bit_offset = 0;
      break;
    case 4096:
      bit_offset = bit_width;
      break;
    default:
      return;
  }
  rect->x = (DISPLAY_WIDTH - bit_width / 2) / 2;
  rect->y = (DISPLAY_HEIGHT - 16) / 2;
  rect->w = bit_width / 2;
  rect->h = 16;

  Platform::DrawBitmap(reward + bit_offset, rect->x, rect->y, rect->w, 16);

}
// vim:ft=cpp:fdm=syntax
