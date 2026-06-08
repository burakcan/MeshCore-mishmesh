/**
 * @file Arduboy2Core.h
 * \brief
 * mishmesh hardware backend for the Arduboy2 "Core" layer.
 *
 * This file REPLACES the upstream MLXXXp/Arduboy2 src/Arduboy2Core.h (which is
 * AVR/ATmega32u4-specific) in place. It declares the SAME class/namespace and
 * the SAME public static method signatures that the vendored Arduboy2Base
 * calls, so the vendored Arduboy2.h's `#include "Arduboy2Core.h"` resolves to
 * this backend with no include hacking. The implementation (Arduboy2Core.cpp)
 * forwards rendering to a mishmesh::Canvas and reads buttons from a latch the
 * runtime sets; everything hardware-specific (SPI/OLED/RGB/timers/USB) is a
 * no-op. This header is mishmesh-authored, not vendored — see ../NOTICE.
 */

#ifndef ARDUBOY2_CORE_H
#define ARDUBOY2_CORE_H

#include <Arduino.h>

namespace mishmesh { class Canvas; }

// ----- AVR compatibility shims -----
// The vendored Arduboy2 expects a handful of AVR macros the nRF52 Adafruit core
// does not provide. Restore exactly what the original <avr/*>/Arduino.h gave.

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// Tell the vendored Arduboy2 an external (non-AVR) core is in use, so it skips
// AVR-only paths (e.g. power_timer0_disable() in flashlight()).
#ifndef ARDUBOY_CORE
#define ARDUBOY_CORE
#endif

// The original Core.h defaults to the production Arduboy build flag.
#ifndef ARDUBOY_10
#define ARDUBOY_10
#endif

// TX LED macros are Leonardo/AVR-only; make them inert no-ops (used only by the
// unused nextFrameDEV()).
#ifndef TXLED0
#define TXLED0 ((void)0)
#endif
#ifndef TXLED1
#define TXLED1 ((void)0)
#endif

#define RGB_ON LOW   /**< For digitially setting an RGB LED on using digitalWriteRGB() */
#define RGB_OFF HIGH /**< For digitially setting an RGB LED off using digitalWriteRGB() */

// RGB LED "pin" names (no-op targets; kept for source compatibility).
#define RED_LED 10
#define GREEN_LED 11
#define BLUE_LED 9

// bit values for button states (must match ArduboyGate's mask, Task 6).
#define LEFT_BUTTON _BV(5)  /**< The Left button value for functions requiring a bitmask */
#define RIGHT_BUTTON _BV(6) /**< The Right button value for functions requiring a bitmask */
#define UP_BUTTON _BV(7)    /**< The Up button value for functions requiring a bitmask */
#define DOWN_BUTTON _BV(4)  /**< The Down button value for functions requiring a bitmask */
#define A_BUTTON _BV(3)     /**< The A button value for functions requiring a bitmask */
#define B_BUTTON _BV(2)     /**< The B button value for functions requiring a bitmask */

// OLED hardware (SSD1306) command constants — kept for source compatibility.
#define OLED_PIXELS_INVERTED 0xA7 // All pixels inverted
#define OLED_PIXELS_NORMAL 0xA6 // All pixels normal

#define OLED_ALL_PIXELS_ON 0xA5 // all pixels on
#define OLED_PIXELS_FROM_RAM 0xA4 // pixels mapped to display RAM contents

#define OLED_VERTICAL_FLIPPED 0xC0 // reversed COM scan direction
#define OLED_VERTICAL_NORMAL 0xC8 // normal COM scan direction

#define OLED_HORIZ_FLIPPED 0xA0 // reversed segment re-map
#define OLED_HORIZ_NORMAL 0xA1 // normal segment re-map

#define WIDTH 128 /**< The width of the display in pixels */
#define HEIGHT 64 /**< The height of the display in pixels */

#define COLUMN_ADDRESS_END (WIDTH - 1) & 127   // 128 pixels wide
#define PAGE_ADDRESS_END ((HEIGHT/8)-1) & 7    // 8 pages high

/** \brief
 * Eliminate the USB stack to free up code space. (Inert on this platform; the
 * applet runtime owns main().)
 */
#define ARDUBOY_NO_USB int main() __attribute__ ((OS_main)); \
int main() { \
  Arduboy2NoUSB::mainNoUSB(); \
  return 0; \
}

// A replacement for the Arduino main() function that eliminates the USB code.
// Used by the ARDUBOY_NO_USB macro.
class Arduboy2NoUSB
{
  friend int main();

  private:
    static void mainNoUSB();
};


/** \brief
 * Lower level functions generally dealing directly with the hardware. On
 * mishmesh these are backed by a Canvas (rendering) and a button latch; the
 * rest are no-ops.
 */
class Arduboy2Core : public Arduboy2NoUSB
{
  friend class Arduboy2Ex;

  public:

    static void idle();
    static void LCDDataMode();
    static void LCDCommandMode();
    static void SPItransfer(uint8_t data);
    static uint8_t SPItransferAndRead(uint8_t data);
    static void displayOff();
    static void displayOn();

    static constexpr uint8_t width() { return WIDTH; }
    static constexpr uint8_t height() { return HEIGHT; }

    static uint8_t buttonsState();

    static void paint8Pixels(uint8_t pixels);
    static void paintScreen(const uint8_t *image);
    static void paintScreen(uint8_t image[], bool clear = false);

    static void blank();
    static void invert(bool inverse);
    static void allPixelsOn(bool on);
    static void flipVertical(bool flipped);
    static void flipHorizontal(bool flipped);
    static void sendLCDCommand(uint8_t command);

    static void setRGBled(uint8_t red, uint8_t green, uint8_t blue);
    static void setRGBled(uint8_t color, uint8_t val);
    static void freeRGBled();
    static void digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue);
    static void digitalWriteRGB(uint8_t color, uint8_t val);

    static void boot();
    static void safeMode();
    static unsigned long generateRandomSeed();
    static void delayShort(uint16_t ms) __attribute__ ((noinline));
    static void exitToBootloader();
};

namespace mishmesh { namespace arduboy {

// Set by the runtime (Task 6) each pass: the Canvas Arduboy2Core::paintScreen
// blits into, and the Arduboy button bitmask Arduboy2Core::buttonsState reports.
void coreSetCanvas(mishmesh::Canvas* canvas);
void coreSetButtons(uint8_t arduboyButtonMask);
// [mishmesh]
// Blit the current sBuffer to the active canvas without a game-step.
// Called by ArduboyRuntime::present() each pass so the display stays fresh
// even on non-step frames (the framework clears the panel buffer every pass).
void coreBlitCurrent();
// [/mishmesh]

}}  // namespace mishmesh::arduboy

#endif
