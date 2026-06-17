// Adapted from awisse/2048-Arduboy (MIT) sketch `2048-Arduboy.ino` @ 038b34bf.
// This is the Arduboy `Platform` implementation + entry points, retargeted to the
// mishmesh Arduboy2 runtime:
//   - setup()/loop() -> game2048Setup()/game2048LoopStep(); the nextFrame() gate,
//     idle(), display(), and the "wait for A" boot loop are removed (ArduboyRuntime
//     gates the sim and blits the framebuffer).
//   - EEPROM is backed by the mishmesh EEPROM image via the Arduino-compatible facade
//     (upstream `#include <EEPROM.h>` -> `<mishmesh/arduboy/ArduboyEeprom.h>`; the
//     non-assignable EEPROM[idx] write is replaced by EEPROM.write()).
//   - Serial/DEBUG blocks omitted (no Serial on the mishmesh companion radio).
#include <Arduboy2.h>
#include <mishmesh/arduboy/ArduboyEeprom.h>
#include "controller.h"
#include "game.h"
#include "draw.h"
#include "defines.h"
#include "platform.h"
#include "font.h"
#include "Game2048.h"

Arduboy2Base s_arduboy;

// [mishmesh] The save record must fit the EEPROM image at base EEPROM_STORAGE_SPACE_START.
static_assert(EEPROM_STORAGE_SPACE_START + 6 + sizeof(GameStateStruct) +
                  sizeof(uint16_t) * DIM * DIM <=
              mishmesh::arduboy::EEPROM_IMAGE_BYTES,
              "2048 save record does not fit the mishmesh EEPROM image");

void game2048Setup() {
  // [mishmesh] Hopper-style no-logo init on a stock Arduboy2Base: beginDoFirst()
  // does boot()/display()/flashlight()/systemButtons()/audio.begin() but skips
  // bootLogo() + waitNoButtons() (which would animate/block the cooperative loop).
  // RNG is already seeded by ArduboyRuntime::begin() (randomSeed(millis())), so
  // s_arduboy.initRandomSeed() — which reads board-specific ADC/pin macros — is omitted.
  s_arduboy.beginDoFirst();
  s_arduboy.setFrameRate(FRAME_RATE);   // informational; the runtime gates externally
  InitGame();
}

void game2048LoopStep() {
  StepGame();
}

void game2048NewGame() {
  // ResetGame() (not NewGame()) — NewGame() early-returns while a game is running,
  // so the applet's "Restart?" confirm must force a fresh board via ResetGame().
  ResetGame();
}

void game2048Persist() {
  // Persist the in-progress board so it resumes from the menu. SaveGame() no-ops while
  // GameState.saved is true (the game clears it on every move), so leaving without
  // having moved writes nothing — only real changes hit the LittleFS-backed store,
  // avoiding needless flash wear. The applet then flushes via saveIfDirty() (itself a
  // no-op unless the EEPROM image actually changed).
  SaveGame();
}

// ---- Platform: input (fed by ArduboyRuntime::pumpButtons -> core buttons state) ----
uint8_t Platform::ButtonState() {
  return s_arduboy.buttonsState();
}

// ---- Platform: drawing (verbatim wrappers over the Arduboy2 framebuffer) ----
void Platform::PutPixel(uint8_t x, uint8_t y, uint8_t colour) {
  s_arduboy.drawPixel(x, y, colour);
}
void Platform::DrawBitmap(const uint8_t* bitmap, int16_t x, int16_t y,
                          uint8_t w, uint8_t h, uint8_t colour) {
  s_arduboy.drawBitmap(x, y, bitmap, w, h, colour);
}
void Platform::DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t colour) {
  s_arduboy.drawLine(x0, y0, x1, y1, colour);
}
void Platform::DrawRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t colour) {
  s_arduboy.drawRect(x, y, w, h, colour);
}
void Platform::DrawFilledRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t colour) {
  s_arduboy.fillRect(x, y, w, h, colour);
}
void Platform::DrawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t colour) {
  s_arduboy.drawCircle(x0, y0, r, colour);
}
void Platform::DrawFilledCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t colour) {
  s_arduboy.fillCircle(x0, y0, r, colour);
}
void Platform::FillScreen(uint8_t colour) { s_arduboy.fillScreen(colour); }
void Platform::Clear() { s_arduboy.clear(); }

// ---- Platform: rng + time ----
int16_t Platform::Random(int16_t i0, int16_t i1) { return random(i0, i1); }
uint32_t Platform::Millis() { return millis(); }

// ---- Platform: EEPROM (mishmesh RAM image via the Arduino-compatible facade) ----
SavedState Platform::ToEEPROM(const uint8_t* bytes, const uint16_t offset, const uint16_t sz) {
  if (EEPROM_STORAGE_SPACE_START + offset + sz > EEPROM.length()) return TooBig;
  for (uint16_t i = 0; i < sz; i++) {
    EEPROM.write(EEPROM_STORAGE_SPACE_START + offset + i, bytes[i]);
  }
  return Saved;
}
SavedState Platform::FromEEPROM(uint8_t* bytes, const uint16_t offset, const uint16_t sz) {
  if (EEPROM_STORAGE_SPACE_START + offset + sz > EEPROM.length()) return TooBig;
  for (uint16_t i = 0; i < sz; i++) {
    bytes[i] = EEPROM.read(EEPROM_STORAGE_SPACE_START + offset + i);
  }
  return Saved;
}
