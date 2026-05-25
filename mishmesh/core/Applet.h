#pragma once

#include <stdint.h>
#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;
class AppletHost;

// Live app/device state applets read. Implemented by the adapter and queried on
// demand, since battery/time/connection change over an applet's lifetime. Keeps
// the framework free of companion-specific types (NodePrefs, RTCClock, board).
struct AppServices {
  virtual ~AppServices() {}
  virtual const char* nodeName() const = 0;
  virtual uint16_t    batteryMillivolts() const = 0;
  virtual uint32_t    epochSeconds() const = 0;   // UNIX seconds; 0 if unknown
};

// Handle through which an applet reaches host/app services. Grows as features land.
struct AppletContext {
  AppletHost*  host = nullptr;
  AppServices* app = nullptr;
};

class Applet {
  const char* _name;
protected:
  explicit Applet(const char* name) : _name(name) {}
public:
  virtual ~Applet() {}

  const char* name() const { return _name; }

  virtual void onStart(AppletContext&) {}   // pushed onto the stack
  virtual void onForeground() {}            // became the top
  virtual void onBackground() {}            // covered by another applet
  virtual void onStop() {}                  // popped off

  // Draw, returning the number of milliseconds until the next wanted render.
  virtual int onRender(Canvas& c) = 0;

  // Return true if the event was consumed; otherwise it bubbles up to the host.
  virtual bool onInput(InputEvent) { return false; }
};

}  // namespace mishmesh
