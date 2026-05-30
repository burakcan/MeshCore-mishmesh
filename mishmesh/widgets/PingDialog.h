#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

// Modal that shows a 0-hop ping in progress, then its result (round-trip time +
// both-direction signal quality). The owning applet drives the state and routes
// input here (NavUp/Down scroll the result; other events bubble up to close).
// Shares its chrome with ConfirmDialog (Modal.h) and its body with ScrollText.
class PingDialog : public Widget {
  ScrollText _text;
public:
  void setWaiting();
  void setReplied(uint32_t rttMs, float snrUs, float snrThem);
  void setTimeout();
  bool needsAnimation() const { return _text.needsAnimation(); }
  bool onInput(InputEvent ev) override { return _text.onInput(ev); }
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
