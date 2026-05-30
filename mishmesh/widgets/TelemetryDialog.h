#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>
#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/core/TelemetryDecode.h>

namespace mishmesh {

// Modal that shows a telemetry request in progress, then the decoded fields.
// Same request->wait->result flow and chrome as PingDialog (Modal.h), with a
// ScrollText body. The owning applet drives the state and routes input here
// (NavUp/Down scroll the result; other events bubble up to close).
class TelemetryDialog : public Widget {
  ScrollText _text;
public:
  void setWaiting();
  void setResult(const TelemetryView& v);
  void setTimeout();
  int  lineCount() const { return _text.count(); }
  bool needsAnimation() const { return _text.needsAnimation(); }
  bool onInput(InputEvent ev) override { return _text.onInput(ev); }
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
