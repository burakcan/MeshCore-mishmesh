#pragma once

#include <stdint.h>

namespace mishmesh {

// Broken-down calendar time. weekday: 0=Sun .. 6=Sat.
struct LocalTime {
  int16_t year;
  uint8_t month;    // 1..12
  uint8_t day;      // 1..31
  uint8_t hour;     // 0..23
  uint8_t minute;   // 0..59
  uint8_t second;   // 0..59
  uint8_t weekday;  // 0..6, 0=Sun
};

// Date presentation, persisted in NodePrefs (0 = DMY default).
//   DMY   -> "01/07/2026"   MDY -> "07/01/2026"   DMonY -> "1 Jul 2026"
enum class DateFormat : uint8_t { DMY = 0, MDY = 1, DMonY = 2, COUNT = 3 };

// Break a UTC epoch into local calendar fields for the given offset (minutes,
// e.g. +5:30 = 330). Clamps at the epoch (never returns pre-1970).
LocalTime applyTz(uint32_t epochUtc, int16_t offsetMinutes);

// Inverse: treat lt as local time at offsetMinutes and return the UTC epoch.
uint32_t composeUtc(const LocalTime& lt, int16_t offsetMinutes);

// Days in month for a proleptic-Gregorian year (handles leap years).
uint8_t daysInMonth(int16_t year, uint8_t month);

// "14:09" (24h) or "2:09 PM" (12h).
void formatClock(char* out, uint16_t cap, const LocalTime& lt, bool fmt12h);

// The full date in the chosen format, no weekday:
//   DMY -> "01/07/2026"   MDY -> "07/01/2026"   DMonY -> "1 Jul 2026"
// Used by the settings chooser (a live example), the panel row, and the editor.
void formatDate(char* out, uint16_t cap, const LocalTime& lt, DateFormat fmt);

// Weekday-prefixed date for the home screen, e.g. "Wed 1 Jul 2026".
void formatShortDate(char* out, uint16_t cap, const LocalTime& lt,
                     DateFormat fmt = DateFormat::DMY);

// Compact chat stamp relative to nowUtc: "14:09" when tUtc is on the same local
// day as now, else "1 Jul 14:09". Empty when tUtc == 0. Honors 12/24h.
void formatStamp(char* out, uint16_t cap, uint32_t tUtc, uint32_t nowUtc,
                 int16_t offsetMinutes, bool fmt12h);

// "UTC", "UTC+05:30", "UTC-08:00".
void formatOffset(char* out, uint16_t cap, int16_t offsetMinutes);

}  // namespace mishmesh
