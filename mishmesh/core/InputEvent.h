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
  char       ch = 0;       // free-text char for keyboard sources, else 0
  bool       repeat = false; // true when emitted by auto-repeat (held button), not a fresh press
};

// Bit position for a semantic button in an InputState mask.
static inline uint16_t maskBit(InputEvent e) {
  return (uint16_t)(1u << (uint8_t)e);
}

// Snapshot of which semantic buttons are currently held (debounced). The host
// rebuilds this once per loop from every InputSource::heldMask(); a real-time
// (game) applet polls it via AppletContext::input() instead of waiting for the
// discrete event stream. Discrete events still fire for edges/Back.
struct InputState {
  uint16_t held = 0;
  bool isDown(InputEvent t) const { return (held & maskBit(t)) != 0; }
};

}  // namespace mishmesh
