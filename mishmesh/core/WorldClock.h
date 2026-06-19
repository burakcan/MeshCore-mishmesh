#pragma once

#include <stdint.h>

namespace mishmesh {

// Civil DST rules, evaluated at render time so the table stays tiny. Rules are
// approximations of the IANA zones (transition hour simplified to local
// standard time); good to the hour everywhere, and to the minute outside the
// one transition hour per year.
enum DstRule : uint8_t {
  DST_NONE = 0,
  DST_US,   // 2nd Sun Mar 02:00 -> 1st Sun Nov 02:00 (northern)
  DST_EU,   // last Sun Mar 01:00 UTC -> last Sun Oct 01:00 UTC
  DST_AU,   // 1st Sun Oct 02:00 -> 1st Sun Apr 03:00 (southern, spans new year)
  DST_NZ,   // last Sun Sep 02:00 -> 1st Sun Apr 03:00
  DST_CL,   // 1st Sun Sep 00:00 -> 1st Sun Apr 00:00 (Chile)
  DST_EG,   // last Fri Apr 00:00 -> last Thu Oct 24:00 (Egypt)
};

struct WorldCity {
  const char* name;
  int16_t     baseOffsetMin;   // standard-time offset from UTC
  uint8_t     dstRule;         // DstRule
};

// APPEND-ONLY table: the world-clock applet persists indices into it, so
// reordering or removing entries silently retargets saved cities.
int worldCityCount();
const WorldCity& worldCity(int i);

// The offset in effect for the city at the given instant (base + DST shift).
int16_t worldCityOffsetNow(int i, uint32_t epochUtc);

}  // namespace mishmesh
