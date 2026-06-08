#pragma once
#include <stdint.h>
#include <string.h>

namespace mishmesh { struct AppletStorage; }

namespace mishmesh { namespace arduboy {

// A small RAM image emulating EEPROM for vendored Arduboy2 + game code on platforms with
// no real EEPROM. Persisted as one blob via AppletStorage. 64 bytes covers Arduboy2 system
// bytes (<16) plus a game's small high-score record.
static constexpr int EEPROM_IMAGE_BYTES = 64;

class EepromImage {
  uint8_t _buf[EEPROM_IMAGE_BYTES];
  bool _dirty;
public:
  EepromImage() : _dirty(false) { memset(_buf, 0, sizeof(_buf)); }

  int length() const { return EEPROM_IMAGE_BYTES; }
  uint8_t read(int addr) const { return inRange(addr) ? _buf[addr] : 0; }
  void write(int addr, uint8_t v) { if (inRange(addr)) { _buf[addr] = v; _dirty = true; } }
  void update(int addr, uint8_t v) { if (inRange(addr) && _buf[addr] != v) { _buf[addr] = v; _dirty = true; } }

  template <typename T> T& get(int addr, T& out) const {
    if (inRange(addr) && addr + (int)sizeof(T) <= EEPROM_IMAGE_BYTES) memcpy(&out, _buf + addr, sizeof(T));
    return out;
  }
  template <typename T> const T& put(int addr, const T& in) {
    if (inRange(addr) && addr + (int)sizeof(T) <= EEPROM_IMAGE_BYTES) { memcpy(_buf + addr, &in, sizeof(T)); _dirty = true; }
    return in;
  }

  bool dirty() const { return _dirty; }
  void saveIfDirtyMarkClean() { _dirty = false; }   // test/seam helper

  void load(AppletStorage* s, const char* key);
  void saveIfDirty(AppletStorage* s, const char* key);

private:
  static bool inRange(int a) { return a >= 0 && a < EEPROM_IMAGE_BYTES; }
};

}}  // namespace mishmesh::arduboy
