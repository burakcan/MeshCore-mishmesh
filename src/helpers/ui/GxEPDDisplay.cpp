
#include "GxEPDDisplay.h"

#ifdef EXP_PIN_BACKLIGHT
  #include <PCA9557.h>
  extern PCA9557 expander;
#endif

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 3
#endif
// [mishmesh] GxEPD2 counts rotation in quarter turns, so our own quarter turns
// just add on to whatever the variant chose as its default.
#define EINK_ROTATION(quarters) (((DISPLAY_ROTATION) + (quarters)) % 4)

#ifdef ESP32
  SPIClass SPI1 = SPIClass(FSPI);
#endif

// Color scheme
ColorVal UIColor::window_bkg = GxEPD_WHITE;
ColorVal UIColor::title_bkg = GxEPD_WHITE;
ColorVal UIColor::title_txt = GxEPD_BLACK;
ColorVal UIColor::primary_txt = GxEPD_BLACK;
ColorVal UIColor::secondary_txt = GxEPD_BLACK;
ColorVal UIColor::warning_txt = GxEPD_BLACK;
ColorVal UIColor::popup_bkg = GxEPD_WHITE;
ColorVal UIColor::popup_txt = GxEPD_BLACK;
ColorVal UIColor::corp_blue = GxEPD_BLACK;


bool GxEPDDisplay::begin() {
  display.epd2.selectSPI(SPI1, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#ifdef ESP32
  SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS);
#else
  SPI1.begin();
#endif
  display.init(115200, true, 2, false);
  display.setRotation(EINK_ROTATION(_rotation));   // [mishmesh] honour a turned panel
  setTextSize(1);  // Default to size 1
  display.setPartialWindow(0, 0, display.width(), display.height());

  display.fillScreen(GxEPD_WHITE);
  display.display(true);
  #if DISP_BACKLIGHT
  digitalWrite(DISP_BACKLIGHT, LOW);
  pinMode(DISP_BACKLIGHT, OUTPUT);
  #endif
  _init = true;
  return true;
}

void GxEPDDisplay::turnOn() {
  if (!_init) begin();
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, HIGH);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, HIGH);
#endif
  _isOn = true;
}

void GxEPDDisplay::turnOff() {
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, LOW);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, LOW);
#endif
  _isOn = false;
}

void GxEPDDisplay::clear() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
}

void GxEPDDisplay::startFrame(ColorVal bkg) {
  display.fillScreen(bkg);
  display.setTextColor(_curr_color = UIColor::primary_txt);
  display_crc.reset();
}

void GxEPDDisplay::setTextSize(int sz) {
  display_crc.update<int>(sz);
  switch(sz) {
    case 1:  // Small
      display.setFont(&FreeSans9pt7b);
      break;
    case 2:  // Medium Bold
      display.setFont(&FreeSansBold12pt7b);
      break;
    case 3:  // Large
      display.setFont(&FreeSans18pt7b);
      break;
    default:
      display.setFont(&FreeSans9pt7b);
      break;
  }
}

void GxEPDDisplay::setColor(ColorVal c) {
  display_crc.update<ColorVal> (c);
  display.setTextColor(_curr_color = c);
}

void GxEPDDisplay::setCursor(int x, int y) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display.setCursor((x+offset_x)*scale_x, (y+offset_y)*scale_y);
}

void GxEPDDisplay::print(const char* str) {
  display_crc.update<char>(str, strlen(str));
  display.print(str);
}

// [mishmesh] Installing a busy callback makes GxEPD2 skip the delay(1) it would
// otherwise run between BUSY polls (GxEPD2_EPD::_waitWhileBusy), and the yield()
// next to it is #ifdef'd to ESP8266/ESP32 - so on nRF52 a refresh becomes a hard
// spin for its whole duration: no scheduler yield, no CPU idle, for hundreds of ms
// several times a second. Pace it ourselves. 1ms still samples buttons far finer
// than the normal loop does.
static void (*s_busy_poll)(const void*) = nullptr;
static const void* s_busy_ctx = nullptr;

static void pacedBusyPoll(const void*) {
  if (s_busy_poll) s_busy_poll(s_busy_ctx);
  delay(1);
}

void GxEPDDisplay::setBusyPoll(void (*cb)(const void*), const void* ctx) {
  s_busy_poll = cb;
  s_busy_ctx = ctx;
  display.epd2.setBusyCallback(cb ? pacedBusyPoll : nullptr, nullptr);
}
// [/mishmesh]

// [mishmesh] The variant's EINK_LOGICAL_* describes the panel at 1x in its default
// orientation. A higher multiple divides that down and magnifies to match, which
// for bitmap glyphs is exact; portrait swaps the two axes. Driven off the macros
// rather than display.width() so it is safe to call before begin(), where the
// rotation has not been applied yet.
void GxEPDDisplay::applyGeometry() {
  const bool portrait = (_rotation % 2) != 0;   // odd turns put the panel on its side
  const int lw = portrait ? EINK_LOGICAL_HEIGHT : EINK_LOGICAL_WIDTH;
  const int lh = portrait ? EINK_LOGICAL_WIDTH  : EINK_LOGICAL_HEIGHT;
  const int eff = effectiveUiScale();
  scale_x = (portrait ? EINK_SCALE_Y : EINK_SCALE_X) * eff;
  scale_y = (portrait ? EINK_SCALE_X : EINK_SCALE_Y) * eff;
  setLogicalSize(lw / eff, lh / eff);
}

// Portrait divides the panel's SHORT edge, so a multiple that is comfortable in
// landscape can leave a width no layout survives: 2x on a 250x122 panel is 61
// logical px, where list labels clip mid-word and the keypad ellipsizes its own
// key captions. Step down rather than offer a size that cannot be read. A square
// panel is equally tight either way, so leave that to the variant.
int GxEPDDisplay::effectiveUiScale() const {
  const bool portrait = (_rotation % 2) != 0;
  const int lw = portrait ? EINK_LOGICAL_HEIGHT : EINK_LOGICAL_WIDTH;
  int eff = _ui_scale;
  if (portrait && lw < EINK_LOGICAL_WIDTH)
    while (eff > 1 && lw / eff < MIN_LOGICAL_WIDTH) eff--;
  return eff;
}

// Hide the interface-size choice where it has nothing to offer, instead of
// letting the settings row claim "Large" while the panel renders Standard.
bool GxEPDDisplay::supportsUiScale() const {
  const bool portrait = (_rotation % 2) != 0;
  const int lw = portrait ? EINK_LOGICAL_HEIGHT : EINK_LOGICAL_WIDTH;
  if (portrait && lw < EINK_LOGICAL_WIDTH)
    return lw / MAX_UI_SCALE >= MIN_LOGICAL_WIDTH;
  return true;
}

void GxEPDDisplay::setUiScale(int mult) {
  if (mult < 1) mult = 1;
  if (mult > MAX_UI_SCALE) mult = MAX_UI_SCALE;
  _ui_scale = mult;
  applyGeometry();
}

void GxEPDDisplay::setDisplayRotation(int quarters) {
  quarters = ((quarters % 4) + 4) % 4;
  if (_rotation == quarters) return;
  _rotation = quarters;
  applyGeometry();
  if (_init) {   // before begin() the rotation is applied there instead
    display.setRotation(EINK_ROTATION(_rotation));
    display.setPartialWindow(0, 0, display.width(), display.height());
    display.fillScreen(GxEPD_WHITE);
    display.display(true);       // the old orientation is still on the glass
    last_display_crc_value = 0;
  }
}
// [/mishmesh]

// [mishmesh] both rect paths go through scaleRect() to avoid double truncation
void GxEPDDisplay::fillRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  int x1, y1, x2, y2;
  scaleRect(x, y, w, h, x1, y1, x2, y2);
  display.fillRect(x1, y1, x2 - x1, y2 - y1, _curr_color);
}

void GxEPDDisplay::drawRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  int x1, y1, x2, y2;
  scaleRect(x, y, w, h, x1, y1, x2, y2);
  display.drawRect(x1, y1, x2 - x1, y2 - y1, _curr_color);
}
// [/mishmesh]

void GxEPDDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display_crc.update<uint8_t>(bits, w * h / 8);
  // Calculate the base position in display coordinates
  uint16_t startX = x * scale_x;
  uint16_t startY = y * scale_y;
  
  // Width in bytes for bitmap processing
  uint16_t widthInBytes = (w + 7) / 8;
  
  // Process the bitmap row by row
  for (uint16_t by = 0; by < h; by++) {
    // Calculate the target y-coordinates for this logical row
    int y1 = startY + (int)(by * scale_y);
    int y2 = startY + (int)((by + 1) * scale_y);
    int block_h = y2 - y1;
    
    // Scan across the row bit by bit
    for (uint16_t bx = 0; bx < w; bx++) {
      // Calculate the target x-coordinates for this logical column
      int x1 = startX + (int)(bx * scale_x);
      int x2 = startX + (int)((bx + 1) * scale_x);
      int block_w = x2 - x1;
      
      // Get the current bit
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      bool bitSet = pgm_read_byte(bits + byteOffset) & bitMask;
      
      // If the bit is set, draw a block of pixels
      if (bitSet) {
        // Draw the block as a filled rectangle
        display.fillRect(x1, y1, block_w, block_h, _curr_color);
      }
    }
  }
}

uint16_t GxEPDDisplay::getTextWidth(const char* str) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return ceil((w + 1) / scale_x);
}

void GxEPDDisplay::endFrame() {
  uint32_t crc = display_crc.finalize();
  if (crc != last_display_crc_value) {
    display.display(true);
    last_display_crc_value = crc;
  }
}
