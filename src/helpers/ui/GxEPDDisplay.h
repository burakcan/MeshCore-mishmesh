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

// [mishmesh] Per-variant panel geometry. The logical canvas the UI draws into is
// configurable so a board can pick an integer scale onto its panel - non-integer
// scales smear the pixel fonts. Defaults reproduce the old hardcoded 128x128.
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
// [/mishmesh]

class GxEPDDisplay : public DisplayDriver {
public:
  static const int MAX_UI_SCALE = 2;   // [mishmesh] 3x leaves too little canvas
  // [mishmesh] narrowest logical canvas the widget set still lays out in
  static const int MIN_LOGICAL_WIDTH = 100;
private:

  GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT> display;
  float scale_x  = EINK_SCALE_X;   // [mishmesh] mutable: see setUiScale()
  float scale_y  = EINK_SCALE_Y;
  const float offset_x = EINK_X_OFFSET;
  const float offset_y = EINK_Y_OFFSET;
  // [mishmesh] geometry the UI asked for; applyGeometry() turns it into the
  // logical canvas size and the logical-to-panel scale.
  int  _ui_scale = 1;
  int  _rotation = 0;       // quarter turns clockwise from the variant default
  void applyGeometry();
  int  effectiveUiScale() const;
  // [/mishmesh]
  bool _init = false;
  bool _isOn = false;
  uint16_t _curr_color;
  CRC32 display_crc;
  int last_display_crc_value = 0;

  // [mishmesh] Scale both edges, then subtract. Scaling w/h independently of x/y
  // truncates twice, so abutting rects leave 1px seams on the panel.
  void scaleRect(int x, int y, int w, int h, int& x1, int& y1, int& x2, int& y2) const {
    x1 = (int)(x * scale_x);
    y1 = (int)(y * scale_y);
    x2 = (int)((x + w) * scale_x);
    y2 = (int)((y + h) * scale_y);
  }
  // [/mishmesh]

public:
  // [mishmesh] logical canvas size comes from the variant
  GxEPDDisplay() : DisplayDriver(EINK_LOGICAL_WIDTH, EINK_LOGICAL_HEIGHT), display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}
  // [/mishmesh]

  bool begin();

  bool isOn() override { return _isOn; }
  bool isEink() override { return true; }
  // [mishmesh] window_bkg is GxEPD_WHITE here; see DisplayDriver::hasLightBackground
  bool hasLightBackground() const override { return true; }
  bool supportsUiScale() const override;
  void setUiScale(int mult) override;
  bool supportsOrientation() const override { return true; }
  void setDisplayRotation(int quarters) override;
  // [/mishmesh]
  // [mishmesh] a panel refresh blocks for hundreds of ms; hand GxEPD2's busy
  // callback through so the UI can keep sampling buttons meanwhile.
  void setBusyPoll(void (*cb)(const void*), const void* ctx) override;
  // [/mishmesh]
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
