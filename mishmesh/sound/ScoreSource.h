#pragma once
#include "ISoundSource.h"

namespace mishmesh { namespace sound {

// Plays an Arduboy "score" byte stream (channel 0 of the obono/ArduboyPlaytune
// format). Replaying the original note/duration bytes reproduces game audio 1:1.
//   0x9n <midiNote>  note-on  (2 bytes)
//   0x8n             note-off (1 byte)
//   0xF0 / 0xE0      end      (1 byte)
//   hi, lo           16-bit big-endian wait in ms (2 bytes)
// A wait emits one event at the current note's frequency (0 if a note-off
// preceded it, i.e. a rest).
class ScoreSource : public ISoundSource {
public:
  void set(const uint8_t* score);
  void reset() override;
  bool nextEvent(uint16_t& freqHz, uint16_t& durMs) override;
private:
  const uint8_t* _score = nullptr;
  const uint8_t* _p = nullptr;
  uint16_t _curFreq = 0;
};

}}  // namespace mishmesh::sound
