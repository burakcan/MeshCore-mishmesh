#pragma once

#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

struct InputSource {
  virtual ~InputSource() {}
  // Fills `out` and returns true when an event is available, else false.
  // The host polls in a loop until it returns false.
  virtual bool poll(InputReport& out) = 0;

  // The host calls this on every foreground change with the current applet's
  // wantsBackRepeat() preference. Sources that can auto-repeat a held button
  // honor it (e.g. the Back button); others ignore it.
  virtual void setHoldRepeat(bool /*enabled*/) {}

  // Bitmask (see maskBit) of semantic buttons this source currently reports as
  // held. Default 0: sources that only emit discrete events (and test fakes)
  // need not implement it. Read after the host has drained poll() this loop.
  virtual uint16_t heldMask() const { return 0; }
};

}  // namespace mishmesh
