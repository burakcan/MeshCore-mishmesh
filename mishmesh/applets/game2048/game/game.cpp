/*
Helper functions to unclutter main .ino file
 */
#include "globals.h"
#include "game.h"
#include "draw.h"
#include "controller.h"
#include "platform.h"
#include "stars.h"
#include "Game2048Anim.h"   // [mishmesh] tile slide animation

static uint16_t board[DIM][DIM]; // [mishmesh] static: avoids clash with MeshCore's global 'board'
static bool s_pendingNewPiece = false;  // [mishmesh] spawn deferred until a slide finishes
uint16_t reached; // Highest rewarded value reached
GameStateStruct GameState;
static int16_t flash;
bool board_full;
bool animating;
uint32_t reward_start;

SavedState LoadGame();
void FlashLogic();
void NewPiece();
void StepReward();
bool MoveTiles(uint8_t direction); // True if something moved
void GameOver(bool winning);
void BoardMask(uint16_t mask);
SavedState CheckSignature(const char* signature, uint16_t offset);

void InitGame() {
  uint8_t loadState = LoadGame();

  if ((loadState != Saved) || (!GameState.running)) {
    NewGame();
    return;
  }

  board_full = false;
  animating = false;

  InitScreen();
  flash = 0;
}

bool StepGame() {

  HandleEvent();

  // [mishmesh] While a slide plays, it owns the board render; defer the merge flash,
  // the final board paint, and the new-piece spawn until it completes.
  if (anim2048Active()) {
    anim2048StepDraw();
    if (!anim2048Active()) {
      if (s_pendingNewPiece) { NewPiece(); s_pendingNewPiece = false; }
      GameState.modified = true;   // repaint the settled board (+ merge flash) next frame
    }
    return true;
  }
  // [/mishmesh]

  FlashLogic();

  if (animating) {
    StepReward();
  } else {
      DrawGameState(GameState.running);
  }

  if (GameState.modified) {
    DrawMap(board);
    if (GameState.score > GameState.highScore) {
      GameState.highScore = GameState.score;
    }
    DrawScore(GameState.score, GameState.highScore, GameState.biggest);
    GameState.modified = false;
    GameState.saved = false;
  }

  if (((reached == 0) && (GameState.biggest >= (1 << TARGET_VALUE))) ||
      ((reached > 0) && (GameState.biggest > reached))) {
    ShowReward();
    reached = GameState.biggest;
  }

  // FIXME: Always true because of DrawGameState ...
  return true;

}

void NewGame() {

  // Don't accidentally destroy a running game. Only
  // start new game if game *NOT* running
  if (GameState.running) {
    return;
  }
  animating = false;
  reached = 0;
  InitScreen();

  BoardMask(0);
  flash = 0;

  GameState.score = 0;
  GameState.running = true;
  GameState.biggest = 0;
  GameState.moving = false;
  GameState.saved = true;

  board_full = false;

  NewPiece();
  NewPiece();
}

void SavedGame() {
  SavedState sstate = LoadGame();
  if (sstate == Saved) {
    // Possible message in the future
    if (animating) {
      InitScreen();
      animating = false;
    }
  }
}

void ResetGame() {
  GameState.running = false;
  animating = false;
  NewGame();
}

void FlashLogic() {

  if (flash > 0) {
    Flash(board);
    if (--flash == 0) {
      BoardMask(0x7FFF);
      GameState.modified = true;
    }
  }

}

void GameOver(bool winning) {
  GameState.running = false;
  SaveGame(); // Mainly to save highscore
  if (winning) {
    ShowReward();
  } else {
    DrawGameOver();
  }
}

void ShowReward() {
  Rectangle rect;
  Platform::Clear();
  DrawReward(GameState.biggest, &rect);
  InitStars(&rect);
  animating = true;
  reward_start = Platform::Millis();
}

void StepReward() {
  if ((Platform::Millis() - reward_start) < REWARD_TIME) {
    StepStars();
  } else {
    StopReward();
  }
}

void StopReward() {
  if (animating) {
    animating = false;
    GameState.modified = true;
    InitScreen();
  }
}

void SaveGame() {

  if (GameState.saved) {
    return;
  }
  GameState.saved = true;
  GameState.moving = false;

  // Save Signature
  if (Platform::ToEEPROM((uint8_t*)signature, 0, 4u) != Saved) {
    return;
  }

  // Save Length
  if (Platform::ToEEPROM((uint8_t*)&eeprom_sz, 4, 2u) != Saved) {
    return;
  }

  // Save GameState
  if (Platform::ToEEPROM((uint8_t*)&GameState, 6, sizeof(GameState)) != Saved)
  {
    return;
  }
  // Save Board
  if (Platform::ToEEPROM((uint8_t*)board, 6 + sizeof(GameState),
      sizeof(board)) != Saved) {
    return;
  }
}

SavedState LoadGame() {

  uint16_t highScore = GameState.highScore;
  uint16_t eeprom_length;
  uint8_t offset; // If length not saved
  SavedState savedState = CheckSignature(signature, 0);

  if (savedState != Saved) {
    return savedState;
  }

  // Check whether two bytes after signature are == eeprom_sz
  savedState = Platform::FromEEPROM((uint8_t*)&eeprom_length, 4, 2u);
  if (savedState != Saved) {
    return savedState;
  }
  offset = (eeprom_length == eeprom_sz) ? 2 : 0;

  savedState = Platform::FromEEPROM((uint8_t*)&GameState, 4 + offset,
    sizeof(GameState));
  if (savedState != Saved) {
    return savedState;
  }

  savedState = Platform::FromEEPROM((uint8_t*)board,
    4 + offset + sizeof(GameState), sizeof(board));
  if (savedState != Saved) {
    return savedState;
  }

  GameState.saved = true;
  GameState.modified = true;
  board_full = !GameState.running;
  // High Score is forever
  if (highScore > GameState.highScore) {
    GameState.highScore = highScore;
  }
  if (GameState.biggest >= (1 << TARGET_VALUE)) {
    reached = GameState.biggest;
  }
  return savedState;
}

void NewPiece() {
  // Insert a new piece in the board.
  //
  uint16_t i, n = 0;
  uint8_t zeroes[PLACES];

  for (i = 0; i < PLACES; i++) {
    if (!board[i & 3][i >> 2]) zeroes[n++] = i;
  }

  board_full = n <= 1;
  if (n == 0) return;

  i = zeroes[Platform::Random(0, n)];
  // Reuse n to save memory
  n = (Platform::Random(0, 100) < HIPROB) ? 2 : 1;
  board[i & 3][i >> 2] = n;

  if ((1U << n) > GameState.biggest) {
    GameState.biggest = 1U << n;
  }

  GameState.modified = true;

}

void ExecuteMove(uint8_t direction) {

  if (!GameState.running) {
    return;
  }
  if (anim2048Active()) {   // [mishmesh] ignore moves while a slide is playing
    return;
  }
  if (!(direction & (INPUT_LEFT | INPUT_RIGHT | INPUT_UP | INPUT_DOWN))) {
    // This can't happen if event dispatched by controller
    EraseRect(98, 8, 29, 8);
    Text::DrawString(U8"BOOM!", 98, 8);
    return;
  }
  anim2048Begin(direction, board);   // [mishmesh] capture trajectories from the pre-move board
  bool moved = MoveTiles(direction);
  if (moved) {
    if (animating) {
      InitScreen();
      animating = false;
    }
    // [mishmesh] spawn the new piece after the slide animation (else immediately)
    if (anim2048Active()) {
      s_pendingNewPiece = true;
    } else {
      NewPiece();
    }
  } else if (board_full) {
    GameOver(false);
  }
}

// Define two functions to access the values of the board.
// boardv inverts the indices for the algorithm to apply
// in the vertical direction.
uint16_t* boardv(uint8_t x, uint8_t y) {
  return &board[y][x];
}
// boardh(i, j) returns the address of the value of board[i][j]
uint16_t* boardh(uint8_t x, uint8_t y) {
  return &board[x][y];
}

bool MoveTiles(uint8_t direction) { // Universal move in all directions
  // Move tiles in any direction
  // Reminder: board[x-axis][y-axis]
  uint16_t sum;
  int8_t i, j, k;  // Loop variables
  int8_t from, to; // Loop limits
  int8_t dir;      // Sign
  uint16_t* (*Board)(uint8_t, uint8_t); // Access function
  bool moved = false; // True if at least one move


  // Depending of the direction chosen by the user, configure the
  // algorithm correspondingly
  if ((direction == INPUT_UP) || (direction == INPUT_LEFT)) {
    from = 0;
    to = DIM - 1;
    dir = 1;
  } else {
    from = DIM - 1;
    to = 0;
    dir = -1;
  }
  if ((direction == INPUT_UP) || (direction == INPUT_DOWN)) {
    Board = boardv;
  } else {
    Board = boardh;
  }

  // Row by row for horizontal, column by column for vertical
  for (j=0; j<DIM; j++) {
    // Shift everything to the "dir"
    for (i=from; dir*i<to; i+=dir) {
      if (*Board(i,j) == 0) {
        for (k=i+dir; dir*k<=dir*to; k+=dir) {
          if (*Board(k,j)) {
            *Board(i,j) = *Board(k,j);
            *Board(k,j) = 0;
            moved = true;
            break;
          }
        }
      }
      if (*Board(i,j)) {
        for (k=i+dir; (dir*k<dir*to) && (*Board(k,j) == 0); k+=dir);
        if (*Board(k,j) == *Board(i,j)) {
          sum = 1 << ++(*Board(i,j));
          if (sum > GameState.biggest) {
            GameState.biggest = sum;
          }
          GameState.score += sum;
          *Board(i,j) |= 0x8000; // Set highest bit for flashing
          flash = FLASH_FRAMES;
          *Board(k,j) = 0;
          moved = true;
        }
      }
    }
  }
  return moved;
}

void BoardMask(uint16_t mask) {
  // Logical and of all board values with mask;
  uint16_t i;
  for (i=0; i<PLACES; i++) {
    *(*board + i) &= mask;
  }
}

void ResetHighScore() {
  GameState.highScore = 0;
}

SavedState CheckSignature(const char* signature, uint16_t offset) {
  char id[4];
  uint8_t i;
  SavedState savedState = Platform::FromEEPROM((uint8_t*)id, offset, 4);

  if (savedState != Saved) {
    return savedState;
  }

  for (i=0; i<4; i++) {
    if (id[i] != signature[i]) {
      return WrongSignature;
    }
  }

  return Saved;
}
// vim:ft=cpp:fdm=syntax
