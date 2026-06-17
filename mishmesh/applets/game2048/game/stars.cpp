#include "platform.h"
#include "stars.h"
#include "utils.h"

uint16_t stars[max_stars];

void InitStars(Rectangle* rect) {
  uint8_t i=0, x, y;
  uint32_t r;

  while (i<max_stars) {
    r = Rand();
    x = XPOS(r);
    y = YPOS(r);
    // No stars inside the message rectangle
    if ((x > rect->x - 3) && (x <= rect->x + rect->w) &&
        (y > rect->y - 3) && (y <= rect->y + rect->h)) continue;
    stars[i] = r;
    i++;
  }
}

void StepStars() {
  uint8_t i, state;
  uint16_t* star;
  int8_t sign;

  for (i=Rand() % 3; i<max_stars; i+=3) {
    star = stars+i;
    sign = ((*star >> 1) & 2) - 1;
    state = (*star & 3) + sign;
    // Set sign bit correctly for next round
    if ((state & 3) == 3) {
      *star &= 0XFFFB;
    } else if ((state & 3) == 0) {
      *star |= 4;
    }
    // Insert new state into star
    *star = (*star & 0xFFFC) | state;
  }
  DrawStars();
}

void DrawStars() {
  uint8_t i, x, y;
  uint16_t star;

  for (i=0; i<max_stars; i++) {
    star=stars[i];
    x = XPOS(star);
    y = YPOS(star);
    switch (star & 3) {
      case hidden:
        Platform::DrawFilledRect(x, y, 3, 3, COLOUR_BLACK);
        break;
      case dimmed:
        Platform::DrawFilledRect(x, y, 3, 3, COLOUR_BLACK);
        Platform::PutPixel(x + 1, y + 1);
        break;
      case halfdimmed:
        Platform::DrawLine(x + 1, y, x + 1, y + 2);
        Platform::DrawLine(x, y + 1, x + 2, y + 1);
        Platform::PutPixel(x + 1, y + 1, COLOUR_BLACK);
        break;
      case full:
        Platform::DrawLine(x + 1, y, x + 1, y + 2);
        Platform::DrawLine(x, y + 1, x + 2, y + 1);
    }
  }
}
// vim:ft=cpp:fdm=syntax
