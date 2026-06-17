#pragma once
#include <mishmesh/arduboy/ArduboyGate.h>

namespace mishmesh {
  struct AppletContext;
  class Canvas;
}

namespace mishmesh { namespace arduboy {

// Arduino-coupled glue layer. Wires ArduboyGate, the Core setters, and EEPROM
// persistence together. The applet (Task 8) owns the game module table and
// calls these methods from its onRender/onInput/onStop hooks.
class ArduboyRuntime {
public:
  // Store ctx, load EEPROM from storage, seed Arduino RNG.
  void begin(mishmesh::AppletContext& ctx, const char* storageKey);

  // Forward canvas to the Core backend.
  void setCanvas(mishmesh::Canvas& c);

  // Compute held+latch button mask and push it to the Core (call before step).
  void pumpButtons();

  // Delegate to gate.
  bool stepDue(uint32_t now);

  // [mishmesh] Set the sim step interval (ms); 0 restores the default ~60Hz.
  void setFrameInterval(uint32_t ms);

  // Blit the current Arduboy framebuffer to the active Canvas (every pass).
  void present();

  // Route Select/SelectLong to pressAB().
  void onButtonEvent(mishmesh::InputEvent ev);

  // Persist EEPROM image if dirty.
  void saveIfDirty();

private:
  ArduboyGate _gate;
  mishmesh::AppletContext* _ctx = nullptr;
  const char* _storageKey = nullptr;
};

}}  // namespace mishmesh::arduboy
