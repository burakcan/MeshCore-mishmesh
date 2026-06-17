#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>

namespace mishmesh {

// A community region radio preset. Sets freq/sf/bw/cr only (never TX power).
struct RadioPreset { const char* name; float freq; uint8_t sf; float bw; uint8_t cr; };

// Deprecated entries dropped; "USA/Canada" carries no "(Recommended)" suffix.
static const RadioPreset PRESETS[] = {
  { "Australia",               915.800f, 10, 250.0f, 5 },
  { "Australia (Narrow)",      916.575f,  7,  62.5f, 8 },
  { "Australia (Mid)",         915.075f,  9, 125.0f, 5 },
  { "Australia: SA, WA",       923.125f,  8,  62.5f, 8 },
  { "Australia: QLD",          923.125f,  8,  62.5f, 5 },
  { "Brazil",                  923.125f,  8,  62.5f, 8 },
  { "EU/UK (Narrow)",          869.618f,  8,  62.5f, 8 },
  { "Czech Republic (Narrow)", 869.432f,  7,  62.5f, 5 },
  { "EU 433MHz (Long Range)",  433.650f,  8,  62.5f, 8 },
  { "Netherlands",             869.618f,  7,  62.5f, 5 },
  { "New Zealand",             917.375f, 11, 250.0f, 5 },
  { "New Zealand (Narrow)",    917.375f,  7,  62.5f, 5 },
  { "Portugal 433",            433.375f,  9,  62.5f, 6 },
  { "Portugal 868",            869.618f,  7,  62.5f, 6 },
  { "Switzerland",             869.618f,  8,  62.5f, 8 },
  { "USA/Canada",              910.525f,  7,  62.5f, 5 },
  { "Vietnam (Narrow)",        920.250f,  8,  62.5f, 5 },
};
static const int PRESET_COUNT = (int)(sizeof(PRESETS) / sizeof(PRESETS[0]));

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
