/**
 * @file Arduboy2Core.cpp
 * \brief
 * mishmesh hardware backend for the Arduboy2 "Core" layer.
 *
 * REPLACES the upstream MLXXXp/Arduboy2 src/Arduboy2Core.cpp (AVR-specific) in
 * place. paintScreen forwards the Arduboy 1bpp column-major framebuffer to the
 * active mishmesh::Canvas; buttonsState returns a latch the runtime sets via
 * coreSetButtons. All SPI/OLED/RGB/USB/boot entry points are no-ops. This file
 * is mishmesh-authored, not vendored — see ../NOTICE.
 */

#include "Arduboy2.h"
#include <string.h>
#include <mishmesh/core/Canvas.h>

// File-local backend state, wired by the runtime (Task 6).
static mishmesh::Canvas* g_canvas = nullptr;
static uint8_t g_buttons = 0;

namespace mishmesh { namespace arduboy {

void coreSetCanvas(mishmesh::Canvas* canvas) { g_canvas = canvas; }
void coreSetButtons(uint8_t arduboyButtonMask) { g_buttons = arduboyButtonMask; }
// [mishmesh]
void coreBlitCurrent() {
  Arduboy2Core::paintScreen(Arduboy2Base::sBuffer);
}
// [/mishmesh]

}}  // namespace mishmesh::arduboy

// ----- rendering / input -----

void Arduboy2Core::paintScreen(const uint8_t *image)
{
  if (g_canvas) g_canvas->blit1bpp(image, WIDTH, HEIGHT);
}

void Arduboy2Core::paintScreen(uint8_t image[], bool clear)
{
  if (g_canvas) g_canvas->blit1bpp(image, WIDTH, HEIGHT);
  if (clear) memset(image, 0, (WIDTH * HEIGHT) / 8);
}

uint8_t Arduboy2Core::buttonsState()
{
  return g_buttons;
}

// ----- lifecycle -----

void Arduboy2Core::boot() {}
void Arduboy2Core::safeMode() {}
void Arduboy2Core::idle() {}

// No-op: the applet host drives a cooperative loop; a blocking delay() would
// stall the mesh. Games pace frames via Arduboy2's nextFrame(), not delayShort.
void Arduboy2Core::delayShort(uint16_t ms) { (void)ms; }

unsigned long Arduboy2Core::generateRandomSeed()
{
  // Entropy is improved by the runtime seeding randomSeed() from the mesh RNG.
  return millis();
}

void Arduboy2Core::exitToBootloader() {}

// ----- display / SPI (no-ops; rendering goes through paintScreen) -----

void Arduboy2Core::LCDDataMode() {}
void Arduboy2Core::LCDCommandMode() {}
void Arduboy2Core::SPItransfer(uint8_t data) { (void)data; }
uint8_t Arduboy2Core::SPItransferAndRead(uint8_t data) { (void)data; return 0; }
void Arduboy2Core::displayOff() {}
void Arduboy2Core::displayOn() {}
void Arduboy2Core::paint8Pixels(uint8_t pixels) { (void)pixels; }
void Arduboy2Core::blank() {}
void Arduboy2Core::invert(bool inverse) { (void)inverse; }
void Arduboy2Core::allPixelsOn(bool on) { (void)on; }
void Arduboy2Core::flipVertical(bool flipped) { (void)flipped; }
void Arduboy2Core::flipHorizontal(bool flipped) { (void)flipped; }
void Arduboy2Core::sendLCDCommand(uint8_t command) { (void)command; }

// ----- RGB LED (no-ops) -----

void Arduboy2Core::setRGBled(uint8_t red, uint8_t green, uint8_t blue) { (void)red; (void)green; (void)blue; }
void Arduboy2Core::setRGBled(uint8_t color, uint8_t val) { (void)color; (void)val; }
void Arduboy2Core::freeRGBled() {}
void Arduboy2Core::digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue) { (void)red; (void)green; (void)blue; }
void Arduboy2Core::digitalWriteRGB(uint8_t color, uint8_t val) { (void)color; (void)val; }

// ----- USB-less main (inert; the firmware owns main()) -----

void Arduboy2NoUSB::mainNoUSB() {}
