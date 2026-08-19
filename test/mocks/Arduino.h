#pragma once

#include <cstdint>
#include <cmath>
#include <stdlib.h>   // real Arduino.h pulls this in; ConfigSerializer.cpp needs atoi/atol/atof
#include "Stream.h"

inline uint32_t g_mock_millis = 0;

using std::isnan;

inline uint32_t millis() {
  return g_mock_millis;
}

inline void delay(uint32_t ms) {
  g_mock_millis += ms;
}
