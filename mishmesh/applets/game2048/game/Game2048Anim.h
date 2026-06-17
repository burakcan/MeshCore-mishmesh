// [mishmesh] Slide animation for tile moves. The vendored 2048 engine moves tiles
// instantly (MoveTiles mutates the board in one pass); this captures per-tile
// from->to trajectories from the *pre-move* board and redraws them interpolated over
// a few gated frames before the final board (and merge flash) is painted.
//
// game.cpp drives it: anim2048Begin() before MoveTiles(), then while anim2048Active()
// StepGame() calls anim2048StepDraw() each frame instead of the normal board draw.
#pragma once
#include <stdint.h>
#include "defines.h"   // DIM, INPUT_*, BOARD_*, TILE_SZ

// One tile's trajectory: board cell (fromA,fromB) -> (toA,toB), drawing exponent `exp`
// (the pre-merge value; hex_digits is indexed by exponent). A merge emits two slides
// with the same destination cell.
struct Slide2048 {
  uint8_t fromA, fromB;
  uint8_t toA, toB;
  uint16_t exp;
};

// Pure trajectory computation (host-testable): fills `out` (capacity DIM*DIM) with one
// entry per non-zero tile for a move in `direction`; returns the count.
uint8_t anim2048ComputeSlides(uint8_t direction, uint16_t board[DIM][DIM], Slide2048* out);

// Capture trajectories for `direction` from the pre-move board and arm the animation
// (no-op arm if nothing actually changes cell).
void anim2048Begin(uint8_t direction, uint16_t board[DIM][DIM]);

// True while a slide is playing.
bool anim2048Active();

// Draw one interpolated frame and advance; clears Active() after the final frame.
void anim2048StepDraw();
