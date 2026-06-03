#pragma once

#include <stdint.h>

struct mf_font_s;

namespace mishmesh {

// mcufont font handle. Fonts are generated bw atlases (see text/fonts/*.c).
typedef struct ::mf_font_s Font;

const Font* fontBody();     // Nokia Cellphone FC size 8 - lists, status bar, body (default)
const Font* fontSubtitle(); // Nokia Cellphone FC size 12 - contact-detail header card only
const Font* fontCaption();  // Tom Thumb 3x6 (4px advance, 6px line) - recessive metadata
const Font* fontNum();     // Nokia Cellphone FC size 24, digits/':'/'.' — clock
const Font* iconFont();    // Pixelarticons 12px - see Icon

// Pixelarticons glyphs in iconFont(). Codepoints assigned by build_icons.py.
enum class Icon : uint16_t {
  Home        = 0xE000,
  Settings    = 0xE001,
  Clock       = 0xE002,
  Menu        = 0xE003,
  User        = 0xE004,
  Users       = 0xE005,
  Mail        = 0xE006,
  Message     = 0xE007,
  Bell        = 0xE008,
  Wifi        = 0xE009,
  BatteryFull = 0xE00A,
  BatteryLow  = 0xE00B,
  Warning     = 0xE00C,
  Back        = 0xE00D,
  ArrowLeft   = 0xE00E,
  Reload      = 0xE00F,
  Power       = 0xE010,
  Gps         = 0xE011,
  Radio       = 0xE012,   // radio/router (repeaters)
  Comment     = 0xE013,   // speech bubble (rooms)
  Chip        = 0xE014,   // cpu (sensors)
  Star        = 0xE015,   // favourite marker
  Search      = 0xE016,   // discover tab
  Plus        = 0xE017,   // create new (messages "New" tab)
  Backspace   = 0xE018,   // keypad delete
  ArrowRight  = 0xE019,   // keypad cursor-right
  Check       = 0xE01A,   // keypad confirm
  Feather     = 0xE01B,   // quill = compose/write
  Zap         = 0xE01C,   // lightning = quick replies
  Bluetooth   = 0xE01D,   // BLE applet
};

}  // namespace mishmesh
