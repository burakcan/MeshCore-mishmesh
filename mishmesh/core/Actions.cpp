#include <mishmesh/core/Actions.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>

namespace mishmesh {

bool toggleBle(AppServices* app, AppletHost* host) {
  if (!app) return false;
  bool now = !app->bleEnabled();
  app->setBleEnabled(now);
  if (host) host->postToast(now ? "Bluetooth on" : "Bluetooth off");
  return now;
}

const char* volumeLevelName(uint8_t level) {
  static const char* const NAMES[4] = { "Mute", "Low", "Mid", "High" };
  return NAMES[level & 3];
}

uint8_t cycleSoundVolume(AppServices* app, sound::SoundEngine* snd, AppletHost* host) {
  if (!snd) return 0;
  uint8_t next = ((uint8_t)snd->volume() + 1) & 3;
  // Route through AppServices so the adapter applies AND persists it; fall
  // back to a live-only change when no app seam is present.
  if (app) app->setSoundVolume(next); else snd->setVolume((sound::VolumeLevel)next);
  if ((sound::VolumeLevel)next != sound::VolumeLevel::Mute)
    snd->play(sound::SoundId::UiConfirm);   // preview at the new level
  if (host) host->postToast(volumeLevelName(next));
  return next;
}

}  // namespace mishmesh
