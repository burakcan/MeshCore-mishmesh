#include "controller.h"
#include "game.h"
#include "defines.h"
#include "platform.h"

uint8_t previousButtons, currentButtons;
uint8_t DebouncedButtons();

bool JustPressed(uint8_t buttons) {
  return ((buttons & currentButtons) && !(buttons & previousButtons));
}

bool JustReleased(uint8_t buttons) {
  return (!(buttons & currentButtons) && (buttons & previousButtons));
}

void HandleEvent() {

  static unsigned long startAPress, startBPress;
  static bool AButtonDown, BButtonDown;
  static int BButtonLongPressCycles;

  previousButtons = currentButtons;
  currentButtons = Platform::ButtonState();

  // Handle A-Button Down Event
  if (JustPressed(INPUT_A)) {
    AButtonDown = true;
    startAPress = Platform::Millis();
    NewGame();
    StopReward();
  }

  // Handle A-Button Up Event
  if (JustReleased(INPUT_A)) {
    AButtonDown = false;
    startAPress = Platform::Millis();
  }

  // Handle A-Button long Press Event
  if (AButtonDown && (Platform::Millis() - startAPress) > LONG_PRESS) {
    startAPress = Platform::Millis();
    SavedGame();
  }

  // Handle B-Button Down Event
  if (JustPressed(INPUT_B)) {
    BButtonDown = true;
    startBPress = Platform::Millis();
    SaveGame();
  }

  // Handle B-Button Up Event
  if (JustReleased(INPUT_B)) {
    BButtonDown = false;
    startBPress = Platform::Millis();
    BButtonLongPressCycles = 0;
  }

  // Handle B-Button long Press Event
  if (BButtonDown && (Platform::Millis() - startBPress) > LONG_PRESS) {
    startBPress = Platform::Millis();
    BButtonLongPressCycles++;
    ResetGame();
  }

  // Special: Erase high score
  if (BButtonDown && AButtonDown && (BButtonLongPressCycles > 4)) {
  #ifdef DEBUG
    DebugPrint("Reset high score");
  #endif
    ResetHighScore();
  }

  uint8_t buttons = DebouncedButtons();
  if (buttons & (INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT)) {
    ExecuteMove(buttons);
  }
}

uint8_t DebouncedButtons() {
  // Wait for release
  static bool pressed;

  if (!currentButtons) {
    pressed = false;
    return 0;
  } else if (pressed) {
    return 0;
  }

  pressed = true;
  return currentButtons;
}
// vim:ft=cpp:fdm=syntax
