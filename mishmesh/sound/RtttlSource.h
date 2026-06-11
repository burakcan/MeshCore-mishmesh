#pragma once
#include "ISoundSource.h"

namespace mishmesh { namespace sound {

// Parses RTTTL ringtone strings (the format the stock UIs use) into note events.
class RtttlSource : public ISoundSource {
public:
  void set(const char* rtttl);   // store + parse header; ready to play from reset()
  void reset() override;
  bool nextEvent(uint16_t& freqHz, uint16_t& durMs) override;
private:
  void parseHeader();
  const char* _song = "";
  const char* _p = "";
  const char* _firstNote = "";
  uint8_t  _defaultDur = 4;
  uint8_t  _defaultOct = 6;
  uint16_t _bpm = 63;
  uint32_t _wholeNoteMs = 0;
};

}}  // namespace mishmesh::sound
