#include <mishmesh/sound/ToneSequencer.h>

namespace mishmesh { namespace sound {

void ToneSequencer::start(ISoundSource* src, VolumeLevel vol, uint32_t nowMs) {
  _src = src; _vol = vol; _playing = true;
  if (_src) _src->reset();
  loadNext(nowMs);
}

void ToneSequencer::stop() {
  if (_playing) { _playing = false; if (_out) _out->silence(); }
}

void ToneSequencer::tick(uint32_t nowMs) {
  if (!_playing) return;
  if ((int32_t)(nowMs - _dueMs) < 0) return;   // current note not done
  loadNext(nowMs);
}

void ToneSequencer::loadNext(uint32_t nowMs) {
  uint16_t f, d;
  if (!_src || !_src->nextEvent(f, d)) { stop(); return; }
  if (_out) { if (f == 0) _out->silence(); else _out->tone(f, _vol); }
  _dueMs = nowMs + d;
}

}}  // namespace mishmesh::sound
