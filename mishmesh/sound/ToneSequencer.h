#pragma once
#include "ISoundSource.h"
#include "ToneOutput.h"

namespace mishmesh { namespace sound {

// Non-blocking playback: pull one (freq,dur) event at a time and hold each for
// its duration. tick() is pumped from the UI loop. No allocation; no blocking.
class ToneSequencer {
public:
  void begin(IToneOutput* out) { _out = out; }
  void start(ISoundSource* src, VolumeLevel vol, uint32_t nowMs);
  void stop();
  bool isPlaying() const { return _playing; }
  void setVolume(VolumeLevel vol) { _vol = vol; }
  void tick(uint32_t nowMs);
private:
  void loadNext(uint32_t nowMs);
  IToneOutput*  _out = nullptr;
  ISoundSource* _src = nullptr;
  VolumeLevel   _vol = VolumeLevel::Mid;
  uint32_t      _dueMs = 0;
  bool          _playing = false;
};

}}  // namespace mishmesh::sound
