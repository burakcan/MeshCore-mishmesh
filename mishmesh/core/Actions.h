#pragma once

#include <stdint.h>

namespace mishmesh {

struct AppServices;
class AppletHost;
namespace sound { class SoundEngine; }

// Shared device actions used by both the settings panels and the quick
// drawer, so behavior and feedback (toasts, previews) can't drift between
// entry points.

// Flip the BLE/companion-link enable state and toast the result. Returns the
// new state. Null-safe: a null app makes it a no-op (no toast either).
bool toggleBle(AppServices* app, AppletHost* host);

// Cycle the master volume Mute -> Low -> Mid -> High -> Mute, persist via the
// app seam (falls back to a live-only engine change), play a preview chirp
// when audible, and toast the level name. Returns the new level (0..3).
uint8_t cycleSoundVolume(AppServices* app, sound::SoundEngine* snd, AppletHost* host);

// "Mute" / "Low" / "Mid" / "High" for a 0..3 level.
const char* volumeLevelName(uint8_t level);

}  // namespace mishmesh
