#include <mishmesh/core/WorldClock.h>
#include <mishmesh/core/TimeFormat.h>

namespace mishmesh {

// Append-only (see header). Names kept short for a 128px row.
static const WorldCity kCities[] = {
  { "UTC",          0,    DST_NONE },
  { "London",       0,    DST_EU },
  { "Lisbon",       0,    DST_EU },
  { "Reykjavik",    0,    DST_NONE },
  { "Paris",        60,   DST_EU },
  { "Berlin",       60,   DST_EU },
  { "Madrid",       60,   DST_EU },
  { "Rome",         60,   DST_EU },
  { "Amsterdam",    60,   DST_EU },
  { "Stockholm",    60,   DST_EU },
  { "Warsaw",       60,   DST_EU },
  { "Lagos",        60,   DST_NONE },
  { "Athens",       120,  DST_EU },
  { "Helsinki",     120,  DST_EU },
  { "Kyiv",         120,  DST_EU },
  { "Cairo",        120,  DST_EG },
  { "Johannesburg", 120,  DST_NONE },
  { "Istanbul",     180,  DST_NONE },
  { "Moscow",       180,  DST_NONE },
  { "Nairobi",      180,  DST_NONE },
  { "Tehran",       210,  DST_NONE },
  { "Dubai",        240,  DST_NONE },
  { "Karachi",      300,  DST_NONE },
  { "Delhi",        330,  DST_NONE },
  { "Kathmandu",    345,  DST_NONE },
  { "Dhaka",        360,  DST_NONE },
  { "Yangon",       390,  DST_NONE },
  { "Bangkok",      420,  DST_NONE },
  { "Jakarta",      420,  DST_NONE },
  { "Singapore",    480,  DST_NONE },
  { "Hong Kong",    480,  DST_NONE },
  { "Beijing",      480,  DST_NONE },
  { "Taipei",       480,  DST_NONE },
  { "Manila",       480,  DST_NONE },
  { "Perth",        480,  DST_NONE },
  { "Tokyo",        540,  DST_NONE },
  { "Seoul",        540,  DST_NONE },
  { "Adelaide",     570,  DST_AU },
  { "Brisbane",     600,  DST_NONE },
  { "Sydney",       600,  DST_AU },
  { "Melbourne",    600,  DST_AU },
  { "Auckland",     720,  DST_NZ },
  { "Honolulu",     -600, DST_NONE },
  { "Anchorage",    -540, DST_US },
  { "Los Angeles",  -480, DST_US },
  { "Vancouver",    -480, DST_US },
  { "Phoenix",      -420, DST_NONE },
  { "Denver",       -420, DST_US },
  { "Chicago",      -360, DST_US },
  { "Mexico City",  -360, DST_NONE },
  { "New York",     -300, DST_US },
  { "Toronto",      -300, DST_US },
  { "Bogota",       -300, DST_NONE },
  { "Lima",         -300, DST_NONE },
  { "Santiago",     -240, DST_CL },
  { "Halifax",      -240, DST_US },
  { "Sao Paulo",    -180, DST_NONE },
  { "Buenos Aires", -180, DST_NONE },
};

int worldCityCount() { return (int)(sizeof(kCities) / sizeof(kCities[0])); }

const WorldCity& worldCity(int i) {
  if (i < 0 || i >= worldCityCount()) i = 0;
  return kCities[i];
}

static uint8_t weekdayOf(int16_t year, uint8_t month, uint8_t day) {
  LocalTime lt{};
  lt.year = year; lt.month = month; lt.day = day;
  return applyTz(composeUtc(lt, 0), 0).weekday;
}

// Day-of-month of the nth (1-based) `wd` (0=Sun) in the month.
static uint8_t nthWeekday(int16_t y, uint8_t m, uint8_t wd, int nth) {
  uint8_t w1 = weekdayOf(y, m, 1);
  return (uint8_t)(1 + ((wd + 7 - w1) % 7) + 7 * (nth - 1));
}

static uint8_t lastWeekday(int16_t y, uint8_t m, uint8_t wd) {
  uint8_t dim = daysInMonth(y, m);
  uint8_t wl = weekdayOf(y, m, dim);
  return (uint8_t)(dim - ((wl + 7 - wd) % 7));
}

static bool afterOrAt(const LocalTime& lt, uint8_t mo, uint8_t d, uint8_t h) {
  if (lt.month != mo) return lt.month > mo;
  if (lt.day != d) return lt.day > d;
  return lt.hour >= h;
}

static bool inDst(uint8_t rule, uint32_t epochUtc, int16_t baseOff) {
  if (rule == DST_NONE) return false;
  if (rule == DST_EU) {   // EU switches at a common UTC instant, not local time
    LocalTime u = applyTz(epochUtc, 0);
    return afterOrAt(u, 3, lastWeekday(u.year, 3, 0), 1) &&
           !afterOrAt(u, 10, lastWeekday(u.year, 10, 0), 1);
  }
  LocalTime lt = applyTz(epochUtc, baseOff);   // local standard time
  switch (rule) {
    case DST_US:
      return afterOrAt(lt, 3, nthWeekday(lt.year, 3, 0, 2), 2) &&
             !afterOrAt(lt, 11, nthWeekday(lt.year, 11, 0, 1), 2);
    case DST_AU:   // southern hemisphere: the DST span crosses new year
      return afterOrAt(lt, 10, nthWeekday(lt.year, 10, 0, 1), 2) ||
             !afterOrAt(lt, 4, nthWeekday(lt.year, 4, 0, 1), 3);
    case DST_NZ:
      return afterOrAt(lt, 9, lastWeekday(lt.year, 9, 0), 2) ||
             !afterOrAt(lt, 4, nthWeekday(lt.year, 4, 0, 1), 3);
    case DST_CL:
      return afterOrAt(lt, 9, nthWeekday(lt.year, 9, 0, 1), 0) ||
             !afterOrAt(lt, 4, nthWeekday(lt.year, 4, 0, 1), 0);
    case DST_EG:   // Friday=5, Thursday=4; ends at the end of the last Thursday
      return afterOrAt(lt, 4, lastWeekday(lt.year, 4, 5), 0) &&
             !afterOrAt(lt, 10, (uint8_t)(lastWeekday(lt.year, 10, 4) + 1), 0);
  }
  return false;
}

int16_t worldCityOffsetNow(int i, uint32_t epochUtc) {
  const WorldCity& c = worldCity(i);
  if (epochUtc == 0) return c.baseOffsetMin;   // clock unset: skip rule math
  return (int16_t)(c.baseOffsetMin + (inDst(c.dstRule, epochUtc, c.baseOffsetMin) ? 60 : 0));
}

}  // namespace mishmesh
