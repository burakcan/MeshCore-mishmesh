#pragma once

#include <cstdint>
#include <cmath>
#include <stdlib.h>   // real Arduino.h pulls these in; ConfigSerializer needs atoi/atol/atof,
#include <stdio.h>    // BaseChatMesh needs sprintf
#include "Stream.h"

// Arduino provides these; the host libc does not.
inline char* ltoa(long value, char* buf, int base) {
  if (base == 10) { sprintf(buf, "%ld", value); return buf; }
  char tmp[34]; int n = 0;
  unsigned long v = (value < 0 && base == 10) ? -(unsigned long)value : (unsigned long)value;
  do { int d = (int)(v % base); tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= base; } while (v);
  char* p = buf;
  while (n) *p++ = tmp[--n];
  *p = 0;
  return buf;
}
inline char* itoa(int value, char* buf, int base) { return ltoa(value, buf, base); }

inline uint32_t g_mock_millis = 0;

using std::isnan;

inline uint32_t millis() {
  return g_mock_millis;
}

inline void delay(uint32_t ms) {
  g_mock_millis += ms;
}
