// Wall, Target, Box, Box on Target
#pragma once
#include "defines.h"
/* typedef enum Piece Piece; */
typedef enum {
  Short_A,
  Long_A,
  Short_B,
  Long_B,
  /* VeryLong_B, */
  Left,
  Right,
  Up,
  Down
} Event;

enum Direction {
  down,
  left,
  right,
  up
};

// Result codes for (To|From)EEPROM
typedef enum {
  Saved,
  NotSaved, // Never been saved.
  WrongOffset,
  TooBig,
  WrongSignature
} SavedState;

constexpr uint8_t gameoverstr[] = "VOUS AVEZ PERDU!";
// vim:ft=cpp
