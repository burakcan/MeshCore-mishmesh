#pragma once
#include <stdint.h>

namespace mishmesh { namespace sound {

// Pull-based note stream. Both RTTTL and Arduboy-score parsers implement this so
// the sequencer is format-agnostic. freqHz == 0 means a rest for durMs.
struct ISoundSource {
  virtual ~ISoundSource() {}
  virtual void reset() = 0;
  virtual bool nextEvent(uint16_t& freqHz, uint16_t& durMs) = 0;  // false at end
};

}}  // namespace mishmesh::sound
