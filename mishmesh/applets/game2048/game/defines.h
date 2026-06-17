#pragma once

#ifndef ARDUINO
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR
#define pgm_read_byte(x) (*((uint8_t*)x))
#define pgm_read_word(x) (*((uint16_t*)x))
#define pgm_read_ptr(x) (*((uintptr_t*)x))
#define strlen_P(x) strlen(x)
#else
#include <avr/pgmspace.h>
//#define pgm_read_ptr pgm_read_word
#endif

#define DEV_DEB

// Conversions
#define U8 (uint8_t*)
#define U16 (uint16_t)
#define I16 (int16_t)

// Game Parameters
#define FRAME_RATE 30
#define FRAME_STEP U16(FRAME_RATE / 10)
#define MAX_STARS 64
#define REWARD_TIME 5000

// Mathematical constants

// Functions
#define XPOS(x) ((x >> 3) & 0x7F)
#define YPOS(x) ((x >> 10) & 0x3F)

// Game maximal value: 1 << MAX_VALUE
#define MAX_VALUE 0xD
#define TARGET_VALUE 0xB

// Probability (in %) of inserting 2 instead of 1 as a new piece
#define HIPROB 10

// Board dimensions
#define DIM 4
#define PLACES DIM * DIM

// Screen dimensions
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

// Board position on the screen
#define BOARD_X 32
#define BOARD_Y 0

// Tile size in pixels
#define TILE_SZ 16

// Position of text on screen
#define SCORE_X 100
#define SCORE_LINE 0
#define HIGHSCORE_LINE 3
#define MAX_LINE 6
#define FONT_STEP 8
#define OVER_LINE 3

// Color codes
#define COLOUR_BLACK 0
#define COLOUR_WHITE 1
#define COLOUR_INVERT 2

// Tile flashing duration in number of frames
#define FLASH_FRAMES 4

// Buttons
#define INPUT_LEFT 32u
#define INPUT_RIGHT 64u
#define INPUT_UP 128u
#define INPUT_DOWN 16u
#define INPUT_A 8u
#define INPUT_B 4u
#define LONG_PRESS 2000

// vim: ft=cpp
