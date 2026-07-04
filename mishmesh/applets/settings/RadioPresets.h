#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>

namespace mishmesh {

// A community region radio preset. Sets freq/sf/bw/cr only (never TX power).
struct RadioPreset { const char* name; float freq; uint8_t sf; float bw; uint8_t cr; };

// The table is defined once in RadioPresets.cpp (extern, not a header-local `static const`)
// so its ~830B of rodata isn't duplicated into every including TU - the BLE build is tight.
extern const RadioPreset PRESETS[];
extern const int PRESET_COUNT;

// Index of the preset whose freq/bw/sf/cr all match, else -1 ("Custom").
inline int matchPreset(float freqMhz, float bwKhz, uint8_t sf, uint8_t cr) {
  for (int i = 0; i < PRESET_COUNT; i++) {
    const RadioPreset& p = PRESETS[i];
    if (p.sf == sf && p.cr == cr &&
        fabsf(p.freq - freqMhz) < 0.005f && fabsf(p.bw - bwKhz) < 0.05f) {
      return i;
    }
  }
  return -1;
}

}  // namespace mishmesh
