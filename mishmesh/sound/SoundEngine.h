#pragma once
#include "ToneOutput.h"
#include "ToneSequencer.h"
#include "RtttlSource.h"
#include "ScoreSource.h"

namespace mishmesh { namespace sound {

enum class SoundCategory : uint8_t { System = 0, Notification, Game, Ui, COUNT };
enum class SoundId : uint8_t;   // defined in Sounds.h

// Single owner of the buzzer. Decides what actually plays: master mute,
// per-category enable, an exclusive game lock, and priority preemption.
class SoundEngine {
public:
  void begin(IToneOutput* out);
  void tick(uint32_t nowMs);

  bool playRtttl(const char* rtttl, SoundCategory cat);
  bool playScore(const uint8_t* score, SoundCategory cat = SoundCategory::Game);
  bool play(SoundId id);
  // Ring at a fixed level regardless of the shared volume (alarm/timer rings:
  // audible even with the system volume dialed to Mute). Master mute still gates.
  bool play(SoundId id, VolumeLevel overrideVol);
  void stop();
  bool isPlaying() const { return _hasPending || _seq.isPlaying(); }

  // Exclusive lock: while held by an owner, only Game plays; others are dropped.
  bool acquireExclusive(const void* owner);
  void releaseExclusive(const void* owner);

  void setVolume(VolumeLevel v);
  VolumeLevel volume() const { return _volume; }
  void setMasterMute(bool on);
  bool masterMute() const { return _mute; }
  void setCategoryEnabled(SoundCategory c, bool on);
  bool categoryEnabled(SoundCategory c) const;
  uint8_t categoryMask() const;            // bit c set => category enabled
  void setCategoryMask(uint8_t mask);

private:
  static int priorityOf(SoundCategory c);  // higher wins
  bool gate(SoundCategory cat) const;
  VolumeLevel effVol() const { return _mute ? VolumeLevel::Mute : _volume; }
  void queue(ISoundSource* src, SoundCategory cat);

  ToneSequencer _seq;
  RtttlSource   _rtttl;
  ScoreSource   _score;
  ISoundSource* _pending = nullptr;
  SoundCategory _pendingCat = SoundCategory::Ui;
  bool          _hasPending = false;
  VolumeLevel   _pendingVol = VolumeLevel::Mid;   // one-shot override, armed by play(id, vol)
  bool          _hasPendingVol = false;
  SoundCategory _curCat = SoundCategory::Ui;
  const void*   _exclusive = nullptr;
  VolumeLevel   _volume = VolumeLevel::Mid;
  bool          _mute = false;
  bool          _enabled[(int)SoundCategory::COUNT] = {};
};

// One engine owns the single buzzer pin. Game code (MyArduboy) reaches it here.
void        setActiveEngine(SoundEngine* e);
SoundEngine* activeEngine();

}}  // namespace mishmesh::sound
