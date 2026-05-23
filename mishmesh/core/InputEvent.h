#pragma once

#include <stdint.h>

namespace mishmesh {

// Device-agnostic input events. Physical buttons/joysticks map onto these.
enum class InputEvent : uint8_t {
  None = 0,
  NavUp,
  NavDown,
  NavLeft,
  NavRight,
  Select,
  Back,
  Cancel,
  SelectLong,
  BackLong,
};

struct InputReport {
  InputEvent event = InputEvent::None;
  char       ch = 0;   // free-text char for keyboard sources, else 0
};

}  // namespace mishmesh
