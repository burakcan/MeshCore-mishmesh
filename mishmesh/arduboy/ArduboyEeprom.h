#pragma once
// mishmesh-authored. The Arduino-EEPROM-compatible global the vendored Arduboy2
// includes. Upstream Arduboy2 does `#include <EEPROM.h>`; on this repo's
// case-insensitive filesystem an `EEPROM.h` next to Task 4's `Eeprom.h` would
// be the SAME file, so per the design's fallback (spec/brief Step 3) the shim is
// named `ArduboyEeprom.h` and the two vendored include sites are patched to it.
//
// It provides an `EEPROM` global object (read/write/update/get/put/length) that
// delegates to a single process-global EepromImage (the RAM shim from Task 4).
// The image + accessor + the `EEPROM` instance are defined in ArduboyEeprom.cpp;
// the runtime (Task 6) load/saves the image via AppletStorage.

#include <stdint.h>
#include <mishmesh/arduboy/Eeprom.h>

namespace mishmesh { namespace arduboy {

// Accessor for the process-global EEPROM image (defined in ArduboyEeprom.cpp).
EepromImage& eeprom();

}}  // namespace mishmesh::arduboy

// Arduino-style EEPROM facade. Stateless: every call forwards to eeprom().
class EEPROMClass {
public:
  uint8_t read(int idx) const { return mishmesh::arduboy::eeprom().read(idx); }
  void write(int idx, uint8_t val) { mishmesh::arduboy::eeprom().write(idx, val); }
  void update(int idx, uint8_t val) { mishmesh::arduboy::eeprom().update(idx, val); }
  template <typename T> T& get(int idx, T& t) const { return mishmesh::arduboy::eeprom().get(idx, t); }
  template <typename T> const T& put(int idx, const T& t) { return mishmesh::arduboy::eeprom().put(idx, t); }
  uint16_t length() const { return (uint16_t)mishmesh::arduboy::eeprom().length(); }
  uint8_t operator[](int idx) const { return read(idx); }
};

// Single instance shared across translation units (defined in ArduboyEeprom.cpp).
extern EEPROMClass EEPROM;
