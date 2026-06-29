#pragma once
#include <cstdint>

namespace mishmesh {

// Once-per-fill latch for the "contacts full" notification. update() returns true
// exactly on the not-full -> full transition, and only when the feature should
// fire (enabled, and overwrite-oldest off - with overwrite on a full store still
// accepts new contacts by evicting the oldest, so "full" is not a problem).
// wasFull tracks raw fullness independent of enabled/overwrite, so enabling the
// setting while already full does not retroactively fire; it fires on the next
// genuine refill transition.
// The first update() only seeds state (primed), so a device that boots with an
// already-full store does NOT fire on every reboot - only a real not-full -> full
// transition while running does.
struct ContactsFullLatch {
  bool wasFull = false;
  bool primed  = false;   // first update() seeds wasFull without firing

  bool update(uint16_t used, uint16_t max, bool enabled, bool overwrite) {
    bool full = (max > 0) && (used >= max);
    bool fire = primed && full && !wasFull && enabled && !overwrite;
    wasFull = full;
    primed  = true;
    return fire;
  }
};

}  // namespace mishmesh
