#include <stdint.h>
#include "platform.h"
#include "utils.h"

uint16_t r = 54167; // Default seed

uint16_t Rand() {
  r = 77 * r + 73;
  return r;
}

void SRand(uint16_t seed) {
  r = seed;
}

uint8_t len(const uint8_t* text) {
  // Compute length of utf-8 string
  uint8_t length=0;

  while (*text++) {
    if ((*text & 0xC0) != 0xC0)
      length++;
  }
  return length;
}

