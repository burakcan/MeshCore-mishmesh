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

bool SH1106Display::begin()
{
  return display.begin(DISPLAY_ADDRESS, true) && i2c_probe(Wire, DISPLAY_ADDRESS);
}

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

void SH1106Display::startFrame(Color bkg)
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

void SH1106Display::setColor(Color c)
{
  _color = (c != 0) ? SH110X_WHITE : SH110X_BLACK;
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
  display.drawBitmap(x, y, bits, w, h, SH110X_WHITE);
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
