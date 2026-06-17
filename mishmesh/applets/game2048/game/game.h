#pragma once

#include <stdint.h>
#include "defines.h"
#include "globals.h"

constexpr char signature[] = "2048";

typedef struct {
  bool running;   // true if running, false if not.
  bool modified;  // true after board is modified. For refresh.
  uint16_t score;      // Current score of game.
  uint16_t highScore;  // Highest (saved) score ever.
  uint16_t biggest;    // Biggest number reached.
  uint8_t moving; // If nonzero, indicates moving direction.
  bool saved; // If state of game is saved in EEPROM
  } GameStateStruct;

// Number of bytes used in EEPROM
constexpr uint16_t eeprom_sz = 0x30;

void InitGame();
bool StepGame();
void NewGame();
void SavedGame();
void ResetGame();
void SaveGame();
void ExecuteMove(uint8_t direction);
void ShowReward();
void StopReward();
void ResetHighScore();

// vim: tabstop=2:softtabstop=2:shiftwidth=2:expandtab:ft=cpp
