#pragma once

#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

struct InputSource {
  virtual ~InputSource() {}
  // Fills `out` and returns true when an event is available, else false.
  // The host polls in a loop until it returns false.
  virtual bool poll(InputReport& out) = 0;
};

}  // namespace mishmesh
