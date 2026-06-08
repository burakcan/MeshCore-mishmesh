#include <mishmesh/applets/StopwatchApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

void StopwatchApplet::toggle(uint32_t now) {
  if (_running) {
    _accum += now - _start;
    _running = false;
  } else {
    _start = now;
    _running = true;
  }
}

uint32_t StopwatchApplet::elapsedMs(uint32_t now) const {
  return _accum + (_running ? now - _start : 0);
}

int StopwatchApplet::onRender(Canvas& c) {
  _now = c.now();
  uint32_t ms = elapsedMs(_now);
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u:%02u.%u",
           (unsigned)(ms / 60000), (unsigned)((ms / 1000) % 60),
           (unsigned)((ms / 100) % 10));
  int nh = c.fontHeight(fontNum());
  int hintH = c.fontHeight(fontBody()) + 2;
  c.drawText(fontNum(), c.width() / 2, (c.height() - hintH - nh) / 2, buf,
             DisplayDriver::LIGHT, TextAlign::Center);
  c.drawText(fontBody(), c.width() / 2, c.height() - hintH,
             _running ? "Select: stop" : "Select: start",
             DisplayDriver::LIGHT, TextAlign::Center);
  return _running ? 50 : 60000;
}

bool StopwatchApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select) {
    toggle(_now);
    return true;
  }
  if (ev == InputEvent::NavDown) {
    reset();
    return true;
  }
  return false;
}

static StopwatchApplet s_stopwatch;
MISHMESH_REGISTER_APPLET_ICON(&s_stopwatch, ::mishmesh::Placement::AppMenu,
                             "Stopwatch", 5, (uint16_t)::mishmesh::Icon::Clock);

}  // namespace mishmesh
