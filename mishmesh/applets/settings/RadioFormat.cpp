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

bool txPaOutputDeciDbm(const TxPaTable* pa, int setting, int16_t& out) {
  if (!pa || !pa->deciDbm) return false;
  int i = setting - pa->firstSetting;
  if (i < 0 || i >= pa->count) return false;
  out = pa->deciDbm[i];
  return true;
}

const char* formatTxPower(char* buf, size_t cap, int setting, const TxPaTable* pa) {
  int16_t out;
  if (!pa) {
    snprintf(buf, cap, "%d dBm", setting);
  } else if (!txPaOutputDeciDbm(pa, setting, out)) {
    snprintf(buf, cap, "%d dBm chip", setting);
  } else if (out % 10 == 0) {
    snprintf(buf, cap, "%d -> ~%d dBm", setting, out / 10);
  } else {
    int mag = out < 0 ? -out : out;
    snprintf(buf, cap, "%d -> ~%s%d.%d dBm", setting, out < 0 ? "-" : "", mag / 10, mag % 10);
  }
  return buf;
}

}  // namespace mishmesh
