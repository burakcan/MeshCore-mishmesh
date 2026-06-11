#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/sound/SoundEngine.h>

namespace mishmesh {

// Minimal test screen for the buzzer: Select cycles the volume level
// (Mute -> Low -> Mid -> High -> Mute), applied live and previewed with a beep.
// Throwaway UI — a proper sound-settings screen comes later. Volume is set on the
// engine only; it does not persist across reboot yet.
class SoundApplet : public Applet {
  sound::SoundEngine* _snd  = nullptr;
  AppServices*        _app  = nullptr;
  AppletHost*         _host = nullptr;
public:
  SoundApplet() : Applet("Sound") {}

  // Pure helper (host-testable): display name for a level.
  static const char* levelName(sound::VolumeLevel v);
  static sound::VolumeLevel nextLevel(sound::VolumeLevel v);

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
