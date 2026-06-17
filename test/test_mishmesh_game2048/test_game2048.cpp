#include <gtest/gtest.h>
#include <mishmesh/arduboy/Eeprom.h>
#include "globals.h"
#include "game.h"
#include "Game2048Anim.h"

// Vendored game globals (defined in game.cpp). `board` is file-static in game.cpp
// (renamed-away from MeshCore's global `board` on device), so it is not observable
// here; the save/load path writes GameState and board in one EEPROM blob via the
// same offsets, so a GameState round-trip exercises the same persistence contract.
extern GameStateStruct GameState;
SavedState LoadGame();   // external linkage in game.cpp

// Host Platform hooks (host_platform.cpp).
void HostClearEeprom();
void HostSetButtons(uint8_t b);

TEST(Game2048Eeprom, ImageIsLargeEnoughForRecord) {
  EXPECT_GE(mishmesh::arduboy::EEPROM_IMAGE_BYTES, 16 + 4 + 2 + 16 + 32);
}

TEST(Game2048Eeprom, SaveThenLoadRestoresGameState) {
  HostClearEeprom();
  // Arrange a known state and persist it.
  GameState = GameStateStruct{};
  GameState.running = true;
  GameState.saved = false;      // SaveGame() early-returns if already saved
  GameState.score = 500;
  GameState.highScore = 1234;
  GameState.biggest = 8;
  SaveGame();

  // Simulate a reboot: wipe live state, then load.
  GameState = GameStateStruct{};

  EXPECT_EQ(LoadGame(), Saved);
  EXPECT_EQ(GameState.highScore, 1234);   // high score is forever
  EXPECT_EQ(GameState.score, 500);
  EXPECT_EQ(GameState.biggest, 8);
}

// Locks the restart fix: the applet's "Restart?" delegates to ResetGame(), which must
// start a fresh board even mid-game. Plain NewGame() early-returns while running, so
// this would fail (score stays 500) if the bridge called NewGame() instead.
TEST(Game2048Restart, ResetGameStartsFreshWhileRunning) {
  HostClearEeprom();
  GameState = GameStateStruct{};
  GameState.running = true;
  GameState.score = 500;
  ResetGame();
  EXPECT_TRUE(GameState.running);   // a fresh game is running
  EXPECT_EQ(GameState.score, 0);    // the in-progress board was actually reset
}

// Locks the "resume from menu" feature: game2048Persist() clears saved + calls SaveGame()
// on a running board; a reload must bring it back as a running game (not a fresh one).
TEST(Game2048Persist, RunningGameResumesAfterReload) {
  HostClearEeprom();
  GameState = GameStateStruct{};
  GameState.running = true;
  GameState.saved = false;   // what game2048Persist() forces before SaveGame()
  GameState.score = 320;
  GameState.biggest = 16;
  SaveGame();

  GameState = GameStateStruct{};   // simulate reboot / re-entering the applet
  EXPECT_EQ(LoadGame(), Saved);
  EXPECT_TRUE(GameState.running);  // resumes an in-progress game, not a fresh one
  EXPECT_EQ(GameState.score, 320);
  EXPECT_EQ(GameState.biggest, 16);
}

// Slide trajectories: a horizontal (LEFT) move that merges the two leading tiles and
// slides a trailing tile in behind them. board[a][b] -> pixel (32+a*16, b*16).
TEST(Game2048Anim, LeftMergeAndSlideTrajectories) {
  uint16_t b[DIM][DIM];
  for (int a = 0; a < DIM; a++) for (int c = 0; c < DIM; c++) b[a][c] = 0;
  b[0][0] = 1; b[1][0] = 1; b[3][0] = 2;   // top row: 2, 2, _, 4
  Slide2048 s[DIM * DIM];
  uint8_t n = anim2048ComputeSlides(INPUT_LEFT, b, s);
  EXPECT_EQ(n, 3);
  // The exp-2 tile slides from cell (3,0) to (1,0).
  bool found = false;
  for (uint8_t i = 0; i < n; i++) {
    if (s[i].fromA == 3 && s[i].fromB == 0) {
      found = true;
      EXPECT_EQ(s[i].toA, 1); EXPECT_EQ(s[i].toB, 0); EXPECT_EQ(s[i].exp, 2);
    }
  }
  EXPECT_TRUE(found);
  // Both exp-1 tiles land on the wall cell (0,0) (the merge target).
  int landed = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (s[i].exp == 1) { EXPECT_EQ(s[i].toA, 0); EXPECT_EQ(s[i].toB, 0); landed++; }
  }
  EXPECT_EQ(landed, 2);
}

// Vertical (UP) move: two tiles in a column merge to the top cell.
TEST(Game2048Anim, UpMergeTrajectory) {
  uint16_t b[DIM][DIM];
  for (int a = 0; a < DIM; a++) for (int c = 0; c < DIM; c++) b[a][c] = 0;
  b[0][0] = 1; b[0][2] = 1;   // left column: tiles at rows 0 and 2
  Slide2048 s[DIM * DIM];
  uint8_t n = anim2048ComputeSlides(INPUT_UP, b, s);
  EXPECT_EQ(n, 2);
  bool found = false;
  for (uint8_t i = 0; i < n; i++) {
    if (s[i].fromA == 0 && s[i].fromB == 2) {
      found = true;
      EXPECT_EQ(s[i].toA, 0); EXPECT_EQ(s[i].toB, 0);
    }
  }
  EXPECT_TRUE(found);
}

TEST(Game2048Eeprom, LoadFromEmptyEepromIsNotSaved) {
  HostClearEeprom();
  GameState = GameStateStruct{};
  EXPECT_NE(LoadGame(), Saved);   // no valid signature -> not restored
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
