#pragma once

#include <stdint.h>
#include <stdio.h>
#include <mishmesh/core/ContactsService.h>   // ContactKind

namespace mishmesh {

// Human label for an ADV_TYPE_* / ContactKind value.
inline const char* contactTypeName(uint8_t t) {
  switch (t) {
    case (uint8_t)ContactKind::Chat:     return "User";
    case (uint8_t)ContactKind::Repeater: return "Repeater";
    case (uint8_t)ContactKind::Room:     return "Room";
    case (uint8_t)ContactKind::Sensor:   return "Sensor";
    default:                             return "Contact";
  }
}

// Compact relative age, e.g. "now", "5m", "2h", "3d".
inline void contactFormatAge(uint32_t secs, char* buf, int n) {
  if (secs < 60)         snprintf(buf, n, "%us", secs);
  else if (secs < 3600)  snprintf(buf, n, "%um", secs / 60);
  else if (secs < 86400) snprintf(buf, n, "%uh", secs / 3600);
  else                   snprintf(buf, n, "%ud", secs / 86400);
}

}  // namespace mishmesh
