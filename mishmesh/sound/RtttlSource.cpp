#include <mishmesh/sound/RtttlSource.h>
#include <mishmesh/sound/NoteTable.h>

namespace mishmesh { namespace sound {

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

void RtttlSource::set(const char* rtttl) {
  _song = rtttl ? rtttl : "";
  parseHeader();
}

void RtttlSource::parseHeader() {
  const char* p = _song;
  _defaultDur = 4; _defaultOct = 6; _bpm = 63;
  while (*p && *p != ':') p++;     // skip name
  if (*p == ':') p++;              // skip ':'
  if (*p == 'd') { p += 2; int n = 0; while (isDigit(*p)) n = n*10 + (*p++ - '0'); if (n > 0) _defaultDur = n; if (*p == ',') p++; }
  if (*p == 'o') { p += 2; int n = *p++ - '0'; if (n >= 3 && n <= 8) _defaultOct = n; if (*p == ',') p++; }
  if (*p == 'b') { p += 2; int n = 0; while (isDigit(*p)) n = n*10 + (*p++ - '0'); if (n > 0) _bpm = n; if (*p == ':') p++; }
  _wholeNoteMs = (60u * 1000u / _bpm) * 4u;
  _firstNote = p;
  _p = p;
}

void RtttlSource::reset() { _p = _firstNote; }

bool RtttlSource::nextEvent(uint16_t& freqHz, uint16_t& durMs) {
  if (!_p || *_p == '\0') return false;

  // duration
  int num = 0;
  while (isDigit(*_p)) num = num*10 + (*_p++ - '0');
  uint32_t dur = num ? (_wholeNoteMs / num) : (_wholeNoteMs / _defaultDur);

  // note -> semitone (0-based; 12 = rest sentinel)
  uint8_t semi = 12;
  switch (*_p) {
    case 'c': semi = 0;  break;
    case 'd': semi = 2;  break;
    case 'e': semi = 4;  break;
    case 'f': semi = 5;  break;
    case 'g': semi = 7;  break;
    case 'a': semi = 9;  break;
    case 'b': semi = 11; break;
    default:  semi = 12; break;   // 'p' or anything else = rest
  }
  if (*_p) _p++;

  if (*_p == '#') { semi++; _p++; }
  if (*_p == '.') { dur += dur/2; _p++; }

  int oct = _defaultOct;
  if (isDigit(*_p)) { oct = *_p - '0'; _p++; }
  if (*_p == '.') { dur += dur/2; _p++; }
  if (*_p == ',') _p++;

  freqHz = (semi >= 12) ? 0 : noteHz((uint8_t)(semi % 12), oct - 4 + (semi/12));
  durMs  = (uint16_t)dur;
  return true;
}

}}  // namespace mishmesh::sound
