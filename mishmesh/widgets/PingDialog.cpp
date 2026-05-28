#include <mishmesh/widgets/PingDialog.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

void PingDialog::setWaiting() { _text.clear(); _text.addLine("Pinging..."); }
void PingDialog::setTimeout() { _text.clear(); _text.addLine("No response"); }

void PingDialog::setReplied(uint32_t rttMs, float snrUs, float snrThem) {
  _text.clear();
  _text.addf("Round trip: %u ms", rttMs);
  _text.addf("SNR us: %.1f dB", (double)snrUs);
  _text.addf("SNR them: %.1f dB", (double)snrThem);
}

void PingDialog::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  Canvas box = drawModalChrome(view);
  const int pad = 4;
  _text.draw(box, pad, pad, box.width() - 2 * pad, box.height() - 2 * pad);
}

}  // namespace mishmesh
