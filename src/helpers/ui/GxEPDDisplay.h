#pragma once

#include <SPI.h>
#include <Wire.h>

#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <CRC32.h>

#include "DisplayDriver.h"

#ifndef EINK_DISPLAY_MODEL
  #define EINK_DISPLAY_MODEL GxEPD2_150_BN
  #ifndef PIN_DISPLAY_CS
    #define PIN_DISPLAY_CS   DISP_CS
    #define PIN_DISPLAY_DC   DISP_DC
    #define PIN_DISPLAY_RST  DISP_RST
    #define PIN_DISPLAY_BUSY DISP_BUSY
  #endif
#endif
#ifndef EINK_LOGICAL_WIDTH
  #define EINK_LOGICAL_WIDTH 128
#endif
#ifndef EINK_LOGICAL_HEIGHT
  #define EINK_LOGICAL_HEIGHT 128
#endif
#ifndef EINK_SCALE_X
  #define EINK_SCALE_X 1.5625f
#endif
#ifndef EINK_SCALE_Y
  #define EINK_SCALE_Y 1.5625f
#endif
#ifndef EINK_X_OFFSET
  #define EINK_X_OFFSET 0
#endif
#ifndef EINK_Y_OFFSET
  #define EINK_Y_OFFSET 10
#endif

class GxEPDDisplay : public DisplayDriver {

  GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT> display;
  const float scale_x  = EINK_SCALE_X;
  const float scale_y  = EINK_SCALE_Y;
  const float offset_x = EINK_X_OFFSET;
  const float offset_y = EINK_Y_OFFSET;
  bool _init = false;
  bool _isOn = false;
  uint16_t _curr_color;
  CRC32 display_crc;
  int last_display_crc_value = 0;

  void scaleRect(int x, int y, int w, int h, int& x1, int& y1, int& x2, int& y2) const {
    x1 = (int)(x * scale_x);
    y1 = (int)(y * scale_y);
    x2 = (int)((x + w) * scale_x);
    y2 = (int)((y + h) * scale_y);
  }

public:
  GxEPDDisplay() : DisplayDriver(EINK_LOGICAL_WIDTH, EINK_LOGICAL_HEIGHT), display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}

  bool begin();

  bool isOn() override { return _isOn; }
  bool isEink() override { return true; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
