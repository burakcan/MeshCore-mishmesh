#include <mishmesh/applets/settings/RadioFormat.h>
#include <mishmesh/core/Applet.h>
#include <stdio.h>

namespace mishmesh {

const char* formatRadioField(char* buf, size_t cap, int idx, const RadioConfig& cfg) {
  switch (idx) {
    case 1: snprintf(buf, cap, "%g MHz", cfg.freqMhz); break;
    case 2: snprintf(buf, cap, "%g kHz", cfg.bwKhz); break;
    case 3: snprintf(buf, cap, "SF%u", (unsigned)cfg.sf); break;
    case 4: snprintf(buf, cap, "CR%u", (unsigned)cfg.cr); break;
    case 5: snprintf(buf, cap, "%d dBm", (int)cfg.txPowerDbm); break;
    default: if (cap) buf[0] = 0; break;
  }
  return buf;
}

}  // namespace mishmesh
