#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>

namespace mishmesh { namespace sound {

void SoundEngine::begin(IToneOutput* out) {
  if (out) out->begin();   // bring up the PWM/pin backend; without this it no-ops
  _seq.begin(out);
  for (int i = 0; i < (int)SoundCategory::COUNT; i++) _enabled[i] = true;
}

int SoundEngine::priorityOf(SoundCategory c) {
  switch (c) {
    case SoundCategory::System:       return 3;
    case SoundCategory::Notification: return 2;
    case SoundCategory::Game:         return 1;
    default:                          return 0;   // Ui
  }
}

bool SoundEngine::gate(SoundCategory cat) const {
  if (_mute) return false;
  if (!_enabled[(int)cat]) return false;
  if (_exclusive && cat != SoundCategory::Game) return false;   // game owns the pin
  // priority preemption against whatever is currently active
  if (_hasPending) {
    if (priorityOf(cat) < priorityOf(_pendingCat)) return false;
  } else if (_seq.isPlaying()) {
    if (priorityOf(cat) < priorityOf(_curCat)) return false;
  }
  return true;
}

void SoundEngine::queue(ISoundSource* src, SoundCategory cat) {
  _pending = src; _pendingCat = cat; _hasPending = true;
  _hasPendingVol = false;   // a normal play drops any armed override
}

bool SoundEngine::playRtttl(const char* rtttl, SoundCategory cat) {
  if (!gate(cat)) return false;
  _rtttl.set(rtttl);
  queue(&_rtttl, cat);
  return true;
}

bool SoundEngine::playScore(const uint8_t* score, SoundCategory cat) {
  if (!gate(cat)) return false;
  _score.set(score);
  queue(&_score, cat);
  return true;
}

void SoundEngine::stop() { _hasPending = false; _seq.stop(); }

void SoundEngine::tick(uint32_t nowMs) {
  if (_hasPending) {
    _hasPending = false;
    _curCat = _pendingCat;
    _seq.start(_pending, _hasPendingVol ? _pendingVol : effVol(), nowMs);
    _hasPendingVol = false;
  }
  _seq.tick(nowMs);
}

bool SoundEngine::acquireExclusive(const void* owner) {
  if (!_exclusive) { _exclusive = owner; return true; }
  return _exclusive == owner;
}

void SoundEngine::releaseExclusive(const void* owner) {
  if (_exclusive == owner) _exclusive = nullptr;
}

void SoundEngine::setVolume(VolumeLevel v) { _volume = v; _seq.setVolume(effVol()); }

void SoundEngine::setMasterMute(bool on) {
  _mute = on;
  if (on) _seq.stop(); else _seq.setVolume(effVol());
}

void SoundEngine::setCategoryEnabled(SoundCategory c, bool on) { _enabled[(int)c] = on; }
bool SoundEngine::categoryEnabled(SoundCategory c) const { return _enabled[(int)c]; }

uint8_t SoundEngine::categoryMask() const {
  uint8_t m = 0;
  for (int i = 0; i < (int)SoundCategory::COUNT; i++) if (_enabled[i]) m |= (1 << i);
  return m;
}

void SoundEngine::setCategoryMask(uint8_t mask) {
  for (int i = 0; i < (int)SoundCategory::COUNT; i++) _enabled[i] = (mask & (1 << i)) != 0;
}

bool SoundEngine::play(SoundId id) {
  const SoundDef* d = soundDef(id);
  if (!d) return false;
  return playRtttl(d->rtttl, d->category);
}

bool SoundEngine::play(SoundId id, VolumeLevel overrideVol) {
  if (!play(id)) return false;
  _pendingVol = overrideVol;
  _hasPendingVol = true;
  return true;
}

static SoundEngine* s_active = nullptr;
void         setActiveEngine(SoundEngine* e) { s_active = e; }
SoundEngine* activeEngine() { return s_active; }

}}  // namespace mishmesh::sound
