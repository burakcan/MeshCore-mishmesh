#pragma once
#include "defines.h"

enum StarState {
  hidden,
  dimmed,
  halfdimmed,
  full
};

typedef struct {
  uint8_t x;
  uint8_t y;
  uint8_t w;
  uint8_t h;
} Rectangle;

constexpr uint16_t max_stars = MAX_STARS;

void InitStars(Rectangle* rect);
void StepStars();
void DrawStars();

// vim:ft=cpp:fdm=syntax
