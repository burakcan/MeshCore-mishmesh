#include "SH1106Display.h"
#include <Adafruit_GrayOLED.h>
#include "Adafruit_SH110X.h"
#include <string.h>

bool SH1106Display::i2c_probe(TwoWire &wire, uint8_t addr)
{
  wire.beginTransmission(addr);
  uint8_t error = wire.endTransmission();
  return (error == 0);
}

// Color scheme
ColorVal UIColor::window_bkg = SH110X_BLACK;
ColorVal UIColor::title_bkg = SH110X_BLACK;
ColorVal UIColor::title_txt = SH110X_WHITE;
ColorVal UIColor::primary_txt = SH110X_WHITE;
ColorVal UIColor::secondary_txt = SH110X_WHITE;
ColorVal UIColor::warning_txt = SH110X_WHITE;
ColorVal UIColor::popup_bkg = SH110X_BLACK;
ColorVal UIColor::popup_txt = SH110X_WHITE;
ColorVal UIColor::corp_blue = SH110X_WHITE;

bool SH1106Display::begin()
{
  // Wire must already be initialised by board.begin() before this is called.
  // Boards with non-standard SH1106 addresses should define DISPLAY_ADDRESS
  // in their variant/platformio configuration. The SA0 strap selects 0x3C or
  // 0x3D and differs between revisions of the same board (e.g. T-Beam
  // Supreme), so fall back to the other address of the pair.
  uint8_t addr = 0;
  if (i2c_probe(Wire, DISPLAY_ADDRESS)) {
    addr = DISPLAY_ADDRESS;
  } else if (i2c_probe(Wire, DISPLAY_ADDRESS ^ 1)) {
    addr = DISPLAY_ADDRESS ^ 1;
  }
  // Run the Adafruit init even when no panel answered: it is what allocates
  // the frame buffer and the I2C device. Skipping it leaves i2c_dev and
  // spi_dev NULL, and UITask::begin() calls turnOn() regardless of our
  // return value, which then dereferences the null spi_dev.
  bool ok = display.begin(addr ? addr : DISPLAY_ADDRESS, true);
  return addr != 0 && ok;
}

// [mishmesh]
void SH1106Display::setBrightness(uint8_t value)
{
  display.oled_command(SH110X_SETCONTRAST);
  display.oled_command(value);
}
// [/mishmesh]

void SH1106Display::turnOn()
{
  display.oled_command(SH110X_DISPLAYON);
  _isOn = true;
  _shadowValid = false;   // [mishmesh] force a full flush on the first frame after wake
}

void SH1106Display::turnOff()
{
  display.oled_command(SH110X_DISPLAYOFF);
  _isOn = false;
}

void SH1106Display::clear()
{
  display.clearDisplay();
  display.display();
  _shadowValid = false;   // [mishmesh] panel cleared outside the frame loop
}

void SH1106Display::startFrame(ColorVal bkg)
{
  display.clearDisplay(); // TODO: apply 'bkg'
  _color = SH110X_WHITE;
  display.setTextColor(_color);
  display.setTextSize(1);
  display.cp437(true); // Use full 256 char 'Code Page 437' font
}

void SH1106Display::setTextSize(int sz)
{
  display.setTextSize(sz);
}

void SH1106Display::setColor(ColorVal c)
{
  _color = c;
  display.setTextColor(_color);
}

void SH1106Display::setCursor(int x, int y)
{
  display.setCursor(x, y);
}

void SH1106Display::print(const char *str)
{
  display.print(str);
}

void SH1106Display::fillRect(int x, int y, int w, int h)
{
  display.fillRect(x, y, w, h, _color);
}

void SH1106Display::drawRect(int x, int y, int w, int h)
{
  display.drawRect(x, y, w, h, _color);
}

void SH1106Display::drawXbm(int x, int y, const uint8_t *bits, int w, int h)
{
  display.drawBitmap(x, y, bits, w, h, _color);
}

uint16_t SH1106Display::getTextWidth(const char *str)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void SH1106Display::endFrame()
{
  // [mishmesh] Skip the blocking ~25ms I2C flush when the composed frame is byte-for-
  // byte what the panel already shows. The compare is a 1KB memcmp (tens of us); the
  // flush it avoids is three orders of magnitude slower and, while it runs, no input
  // is polled. Animations still flush - only genuinely identical frames are skipped.
  uint8_t* buf = display.getBuffer();
  if (buf != nullptr && _shadowValid && memcmp(buf, _shadow, FB_SIZE) == 0) return;
  // Bump I2C only around the blocking pixel push, then restore. The RTC shares this
  // Wire bus (target.cpp), so it must never run at fast-mode-plus; the bracket
  // confines the fast clock to the framebuffer transfer. Halves the flush.
#ifdef MISHMESH_FAST_FLUSH_HZ
  Wire.setClock(MISHMESH_FAST_FLUSH_HZ);
#endif
  display.display();
#ifdef MISHMESH_FAST_FLUSH_HZ
  Wire.setClock(MISHMESH_NORMAL_I2C_HZ);
#endif
  if (buf != nullptr) { memcpy(_shadow, buf, FB_SIZE); _shadowValid = true; }
  // [/mishmesh]
}

// [mishmesh] Fast full-screen 1bpp blit: the Adafruit SH1106 buffer is the same
// column-major page format as the source, so copy 1KB directly instead of per-pixel.
void SH1106Display::blitColumnMajor1bpp(const uint8_t* buf, int w, int h) {
  if (buf == nullptr) return;
  uint8_t* dst = display.getBuffer();
  if (dst == nullptr || w != width() || h != height()) {
    DisplayDriver::blitColumnMajor1bpp(buf, w, h);   // portable fallback on mismatch
    return;
  }
  memcpy(dst, buf, (size_t)(w * h) / 8);
}
// [/mishmesh]
