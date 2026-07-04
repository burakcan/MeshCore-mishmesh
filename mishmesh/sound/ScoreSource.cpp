#include <mishmesh/sound/ScoreSource.h>
#include <mishmesh/sound/NoteTable.h>

namespace mishmesh { namespace sound {

// Single definition for the NoteTable.h extern (see header).
const uint16_t kOctave4Hz[12] = {
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};

void ScoreSource::set(const uint8_t* score) { _score = score; reset(); }

void ScoreSource::reset() { _p = _score; _curFreq = 0; }

bool ScoreSource::nextEvent(uint16_t& freqHz, uint16_t& durMs) {
  if (!_p) return false;
  for (;;) {
    uint8_t b = *_p;
    if (b == 0xF0 || b == 0xE0) { _p = nullptr; return false; }   // end
    if ((b & 0xF0) == 0x90) {                                     // note-on
      uint8_t note = _p[1];
      _curFreq = noteHz(note % 12, note/12 - 5);
      _p += 2;
      continue;
    }
    if ((b & 0xF0) == 0x80) {                                     // note-off
      _curFreq = 0;
      _p += 1;
      continue;
    }
    // otherwise: 16-bit big-endian wait
    uint16_t ms = (uint16_t)((b << 8) | _p[1]);
    _p += 2;
    freqHz = _curFreq;
    durMs  = ms;
    return true;
  }
}

}}  // namespace mishmesh::sound
