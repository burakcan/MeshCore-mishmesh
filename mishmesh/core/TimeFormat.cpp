#include <mishmesh/core/TimeFormat.h>
#include <stdio.h>

namespace mishmesh {

// Howard Hinnant's civil<->days algorithms (days relative to 1970-01-01).
static int64_t daysFromCivil(int16_t y, uint8_t m, uint8_t d) {
  int yy = y - (m <= 2);
  int era = (yy >= 0 ? yy : yy - 399) / 400;
  int yoe = yy - era * 400;
  int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + doe - 719468;
}

static void civilFromDays(int64_t z, int16_t& y, uint8_t& m, uint8_t& d) {
  z += 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  int64_t doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t yy = yoe + era * 400;
  int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int64_t mp = (5 * doy + 2) / 153;
  d = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
  m = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
  y = (int16_t)(yy + (m <= 2));
}

static LocalTime breakdown(uint32_t epoch) {
  LocalTime t;
  int64_t days = epoch / 86400;
  uint32_t secs = epoch % 86400;
  civilFromDays(days, t.year, t.month, t.day);
  t.hour = (uint8_t)(secs / 3600);
  t.minute = (uint8_t)((secs % 3600) / 60);
  t.second = (uint8_t)(secs % 60);
  t.weekday = (uint8_t)(((days % 7) + 4 + 7) % 7);  // 1970-01-01 = Thu = 4
  return t;
}

LocalTime applyTz(uint32_t epochUtc, int16_t offsetMinutes) {
  int64_t local = (int64_t)epochUtc + (int64_t)offsetMinutes * 60;
  if (local < 0) local = 0;
  return breakdown((uint32_t)local);
}

uint32_t composeUtc(const LocalTime& lt, int16_t offsetMinutes) {
  int64_t days = daysFromCivil(lt.year, lt.month, lt.day);
  int64_t localSecs = days * 86400 + lt.hour * 3600 + lt.minute * 60 + lt.second;
  int64_t utc = localSecs - (int64_t)offsetMinutes * 60;
  if (utc < 0) utc = 0;
  return (uint32_t)utc;
}

uint8_t daysInMonth(int16_t year, uint8_t month) {
  static const uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 30;
  if (month == 2) {
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  return DAYS[month - 1];
}

void formatClock(char* out, uint16_t cap, const LocalTime& lt, bool fmt12h) {
  if (!fmt12h) {
    snprintf(out, cap, "%02u:%02u", lt.hour, lt.minute);
    return;
  }
  uint8_t h = lt.hour % 12;
  if (h == 0) h = 12;
  snprintf(out, cap, "%u:%02u %s", h, lt.minute, lt.hour < 12 ? "AM" : "PM");
}

const char* const MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void formatDate(char* out, uint16_t cap, const LocalTime& lt, DateFormat fmt) {
  switch (fmt) {
    case DateFormat::MDY:
      snprintf(out, cap, "%02u/%02u/%u", lt.month, lt.day, lt.year);
      break;
    case DateFormat::DMonY: {
      const char* mo = (lt.month >= 1 && lt.month <= 12) ? MONTHS[lt.month - 1] : "";
      snprintf(out, cap, "%u %s %u", lt.day, mo, lt.year);
      break;
    }
    default:   // DMY
      snprintf(out, cap, "%02u/%02u/%u", lt.day, lt.month, lt.year);
      break;
  }
}

void formatShortDate(char* out, uint16_t cap, const LocalTime& lt, DateFormat fmt) {
  static const char* WD[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* wd = lt.weekday < 7 ? WD[lt.weekday] : "";
  char d[24];
  formatDate(d, sizeof(d), lt, fmt);
  snprintf(out, cap, "%s %s", wd, d);
}

void formatStamp(char* out, uint16_t cap, uint32_t tUtc, uint32_t nowUtc,
                 int16_t offsetMinutes, bool fmt12h) {
  if (cap == 0) return;
  if (tUtc == 0) { out[0] = 0; return; }
  LocalTime lt = applyTz(tUtc, offsetMinutes);
  char clk[12];
  formatClock(clk, sizeof(clk), lt, fmt12h);
  bool sameDay = true;   // unknown "now" -> just the time
  if (nowUtc) {
    LocalTime nl = applyTz(nowUtc, offsetMinutes);
    sameDay = (nl.year == lt.year && nl.month == lt.month && nl.day == lt.day);
  }
  if (sameDay) { snprintf(out, cap, "%s", clk); return; }
  const char* mo = (lt.month >= 1 && lt.month <= 12) ? MONTHS[lt.month - 1] : "";
  snprintf(out, cap, "%u %s %s", lt.day, mo, clk);
}

void formatOffset(char* out, uint16_t cap, int16_t offsetMinutes) {
  if (offsetMinutes == 0) { snprintf(out, cap, "UTC"); return; }
  char sign = offsetMinutes < 0 ? '-' : '+';
  int mag = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
  snprintf(out, cap, "UTC%c%02d:%02d", sign, mag / 60, mag % 60);
}

}  // namespace mishmesh
