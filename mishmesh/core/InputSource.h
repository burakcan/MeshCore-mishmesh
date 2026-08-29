#pragma once

#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

// Everything frame-dependent happens in here: pin to semantic direction,
// rotation, debounce. The host routes events and pushes policy down; it never
// transforms an event. That split matters because by the time an event reaches
// the host, a joystick push and a labelled button that emits NavDown look
// identical - so a transformation applied there cannot tell them apart.
//
// heldMask() is a second channel, for real-time consumers that need to know a
// key is down rather than that it went down. A source implementing it must
// report the same frame its poll() events are in, and must not convert again.
struct InputSource {
  virtual ~InputSource() {}
  // Fills `out` and returns true when an event is available, else false.
  // The host polls in a loop until it returns false.
  virtual bool poll(InputReport& out) = 0;

  // Which events auto-repeat while held, as a maskBit() bitmask. Pushed by the
  // host on every foreground change: a list wants a held direction to scroll, a
  // confirm dialog wants nothing to repeat, and only the screen knows which.
  virtual void setRepeatMask(uint16_t /*mask*/) {}

  // Quarter turns the UI has been rotated by. Sources whose directions are
  // physically mounted turn with it; everything else ignores this - see the
  // note above on why this can't be centralized in the host.
  virtual void setRotation(int /*quarters*/) {}

  // Bitmask (see maskBit) of semantic buttons this source currently reports as
  // held. Default 0: sources that only emit discrete events (and test fakes)
  // need not implement it. Read after the host has drained poll() this loop.
  virtual uint16_t heldMask() const { return 0; }
};

}  // namespace mishmesh
