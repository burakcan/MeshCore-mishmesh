#pragma once

#include <stdint.h>
#include <string.h>

namespace mishmesh {

// Last few admin CLI replies, keyed by sender. More than one slot so a reply
// from another contact (say one the phone app asked for) can't clobber the one
// a screen is waiting on. Value member of the adapter; no allocation.
class CliReplyRing {
public:
  static const int REPLY_MAX = 168;   // one TXT_TYPE_CLI_DATA text frame, with NUL
  static const int SLOTS = 4;
  static const int KEY_LEN = 6;       // the UI's contact handle is a pubkey prefix

  // Longer text is cut to fit. Every reply bumps seq(), wanted or not.
  void latch(const uint8_t* pubKey, const char* text) {
    Slot& s = _slots[_next];
    memcpy(s.key, pubKey, KEY_LEN);
    strncpy(s.resp, text ? text : "", sizeof(s.resp) - 1);
    s.resp[sizeof(s.resp) - 1] = 0;
    s.seq = ++_seq;
    _next = (_next + 1) % SLOTS;
  }

  // Monotonic; 0 = nothing received yet.
  uint32_t seq() const { return _seq; }

  // Newest reply from pubKey that arrived after afterSeq, or null. The pointer
  // stays valid until SLOTS further replies have landed.
  // noinline: the caller is a virtual override reached through a this-adjusting
  // thunk too, and the compiler otherwise unrolls this loop into both copies.
  __attribute__((noinline)) const char* newest(const uint8_t* pubKey, uint32_t afterSeq) const {
    const Slot* best = nullptr;
    for (int i = 0; i < SLOTS; i++) {
      const Slot& s = _slots[i];
      if (s.seq > afterSeq && memcmp(s.key, pubKey, KEY_LEN) == 0 &&
          (best == nullptr || s.seq > best->seq)) {
        best = &s;
      }
    }
    return best ? best->resp : nullptr;
  }

private:
  struct Slot {
    uint8_t  key[KEY_LEN];
    char     resp[REPLY_MAX];
    uint32_t seq;
  };
  Slot     _slots[SLOTS] = {};
  uint32_t _seq = 0;
  int      _next = 0;
};

}  // namespace mishmesh
