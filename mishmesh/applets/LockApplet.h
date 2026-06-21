#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

// Screen lock. Activated by a triple-Back on the home screen (HomeApplet pushes
// this). While locked, the home screen shows through untouched - but this applet
// is foreground and swallows every input, so nothing navigates. The first press
// surfaces an unlock challenge (a padlock + three pips); three Back presses fill
// the pips and unlock, popping this applet. It renders as an overlay (home draws
// beneath) and opts into keepOnWake so a sleep/wake never drops the lock.
class LockApplet : public Applet {
public:
  LockApplet() : Applet("Lock") {}

  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool isOverlay() const override { return true; }
  bool keepOnWake() const override { return true; }

  // test seams
  bool lockingShownForTest() const { return _mode == Locking; }
  bool challengeShownForTest() const { return _mode == Challenge; }
  int  pipsForTest() const { return _pips; }

private:
  // Locking: the shut-padlock "Locked" flourish shown right after engaging.
  // Resting: home visible, input gated. Challenge: padlock + pips. Unlocking:
  // the shackle-swings-open flourish before we pop.
  enum Mode : uint8_t { Locking, Resting, Challenge, Unlocking };

  // The challenge auto-dismisses back to the resting (home-visible) state after
  // this long without a press, so a stray touch never leaves the lock UI up.
  static const uint32_t CHALLENGE_TIMEOUT_MS = 4000;
  static const uint32_t LOCK_ANIM_MS         = 420;
  static const uint32_t UNLOCK_ANIM_MS       = 520;

  // Padlock centered at cx with its body top at bodyTop. open01 in [0,1] swings
  // the shackle from shut (0) to fully open (1).
  void drawPadlock(Canvas& c, int cx, int bodyTop, float open01);

  AppletHost*         _host  = nullptr;
  sound::SoundEngine* _sound = nullptr;
  Mode     _mode  = Locking;
  uint8_t  _pips  = 0;
  uint8_t  _flash = 0;        // brief emphasis frames on the pip that just filled
  uint32_t _lastMs = 0;      // last input time, for the challenge idle-timeout
  uint32_t _animAt = 0;      // start of the lock/unlock flourish
};

}  // namespace mishmesh
