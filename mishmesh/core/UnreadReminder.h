// mishmesh/core/UnreadReminder.h
#pragma once
#include <stdint.h>

namespace mishmesh {

// Repeating "you still have unread messages" chirp, for a node that lives in a
// backpack rather than in your hand. Armed by an arriving message that actually
// alerted (muted / mentions-only chats never call noteArrival), it asks for a
// beep once per interval until one of three things happens: the unread count
// drops to zero (the chat was read), the user presses any key, or the ring-out
// window expires so a forgotten node doesn't chirp all day.
//
// Pure logic - the adapter owns the clock, the unread count and the buzzer.
class UnreadReminder {
public:
  // intervalSec 0 disables the feature; stopAfterSec 0 means never ring out.
  void configure(uint16_t intervalSec, uint16_t stopAfterSec);

  void noteArrival(uint32_t now);   // an alerting message just played its tone

  // Call every loop pass. lastInputMs is the host's last input activity (a wake
  // press counts); any input newer than the arming instant counts as "user saw
  // it, stop nagging".
  bool tick(uint32_t now, uint16_t notifyUnread, uint32_t lastInputMs);

  bool armed() const { return _armed; }

private:
  uint32_t _intervalMs = 0;
  uint32_t _stopMs = 0;
  uint32_t _armedAt = 0;
  uint32_t _lastBeep = 0;
  bool     _armed = false;
};

}  // namespace mishmesh
