#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ClockService.h>

namespace mishmesh {

// Full-screen "timer done / alarm" banner, pushed by the adapter when the clock
// engine fires (even while asleep or inside another applet). Re-chirps every few
// seconds while showing; any key dismisses; rings out on its own eventually so a
// forgotten alarm doesn't chirp all day. A reused singleton (clockAlertApplet()).
class ClockAlertApplet : public Applet {
public:
  ClockAlertApplet() : Applet("ClockAlert") {}

  void raise(ClockEvent kind) { _kind = kind; _raisedAt = 0; _lastBeep = 0; }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  ClockEvent kindForTest() const { return _kind; }

private:
  void dismiss();

  AppletHost*  _host = nullptr;
  AppServices* _app = nullptr;
  sound::SoundEngine* _sound = nullptr;
  ClockEvent _kind = ClockEvent::None;
  uint32_t   _raisedAt = 0;   // 0 => stamp on next render (ring-out base)
  uint32_t   _lastBeep = 0;
};

ClockAlertApplet& clockAlertApplet();

}  // namespace mishmesh
