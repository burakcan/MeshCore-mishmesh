#pragma once

#include "DisplayDriver.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#define SH110X_NO_SPLASH
#include <Adafruit_SH110X.h>

#ifndef PIN_OLED_RESET
#define PIN_OLED_RESET -1
#endif

#ifndef DISPLAY_ADDRESS
#define DISPLAY_ADDRESS 0x3C
#endif

class SH1106Display : public DisplayDriver
{
  Adafruit_SH1106G display;
  bool _isOn;
  uint8_t _color;

  // [mishmesh] frame-skip: a copy of the buffer last actually flushed to the panel.
  // endFrame() compares against it and skips the blocking ~25ms I2C transfer when the
  // composed frame is unchanged - a static screen (e.g. a confirm modal) otherwise
  // flushes every frame, and that blocking window starves input polling.
  static const int FB_SIZE = 128 * 8;   // 128x64 mono: 1 byte = 8 vertical pixels
  uint8_t _shadow[FB_SIZE];
  bool _shadowValid;
  // [/mishmesh]

  bool i2c_probe(TwoWire &wire, uint8_t addr);

public:
  SH1106Display() : DisplayDriver(128, 64), display(128, 64, &Wire, PIN_OLED_RESET) { _isOn = false; _shadowValid = false; }
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char *str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t *bits, int w, int h) override;
  uint16_t getTextWidth(const char *str) override;
  void endFrame() override;
  // [mishmesh]
  void blitColumnMajor1bpp(const uint8_t* buf, int w, int h) override;
  // [/mishmesh]
};
