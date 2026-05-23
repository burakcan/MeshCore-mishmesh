#pragma once

#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;
class AppletHost;

// Handle through which an applet reaches host/app services. Grows as features land.
struct AppletContext {
  AppletHost* host = nullptr;
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
