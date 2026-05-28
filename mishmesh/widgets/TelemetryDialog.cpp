#include <mishmesh/widgets/TelemetryDialog.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/core/Canvas.h>
#include <helpers/sensors/LPPDataHelpers.h>
#include <stdio.h>

namespace mishmesh {

void TelemetryDialog::setWaiting() { _text.clear(); _text.addLine("Requesting..."); }
void TelemetryDialog::setTimeout() { _text.clear(); _text.addLine("No response"); }

void TelemetryDialog::setResult(const TelemetryView& v) {
  _text.clear();
  if (v.count == 0) { _text.addLine("No fields"); return; }
  char val[24];
  for (int i = 0; i < v.count; i++) {
    const TelemetryField& f = v.fields[i];
    formatField(f, val, sizeof(val));
    // GPS coords are too wide for the narrow modal; give them their own line so
    // they aren't ellipsized (mirrors the "Public key:" split in view-details).
    if (f.type == LPP_GPS) {
      _text.addf("%s ch%d:", telemTypeName(f.type), f.channel);
      _text.addf(" %s", val);
    } else {
      _text.addf("%s ch%d: %s", telemTypeName(f.type), f.channel, val);
    }
  }
}

void TelemetryDialog::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  Canvas box = drawModalChrome(view);
  const int pad = 4;
  _text.draw(box, pad, pad, box.width() - 2 * pad, box.height() - 2 * pad);
}

}  // namespace mishmesh
