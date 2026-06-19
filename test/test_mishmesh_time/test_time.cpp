#include <gtest/gtest.h>
#include <mishmesh/core/TimeFormat.h>
#include <string.h>

using namespace mishmesh;

// 2026-07-01 14:09:00 UTC = 1782914940 (a Wednesday)
static const uint32_t kEpoch = 1782914940u;

TEST(TimeFormat, ApplyTzZeroIsUtc) {
  LocalTime t = applyTz(kEpoch, 0);
  EXPECT_EQ(2026, t.year);
  EXPECT_EQ(7,    t.month);
  EXPECT_EQ(1,    t.day);
  EXPECT_EQ(14,   t.hour);
  EXPECT_EQ(9,    t.minute);
  EXPECT_EQ(3,    t.weekday);   // 2026-07-01 is a Wednesday
}

TEST(TimeFormat, ApplyTzPositiveOffsetRollsDay) {
  // 14:09 UTC + 5:30 = 19:39 same day
  LocalTime t = applyTz(kEpoch, 330);
  EXPECT_EQ(19, t.hour);
  EXPECT_EQ(39, t.minute);
  EXPECT_EQ(1,  t.day);
}

TEST(TimeFormat, ApplyTzNegativeOffsetRollsBackADay) {
  // 2026-07-01 02:09 UTC - 3h = 2026-06-30 23:09
  LocalTime t = applyTz(kEpoch - 12 * 3600, -180);
  EXPECT_EQ(6,  t.month);
  EXPECT_EQ(30, t.day);
  EXPECT_EQ(23, t.hour);
}

TEST(TimeFormat, ComposeUtcRoundTrips) {
  LocalTime local = applyTz(kEpoch, 330);
  EXPECT_EQ(kEpoch, composeUtc(local, 330));
}

TEST(TimeFormat, DaysInMonthLeapYear) {
  EXPECT_EQ(29, daysInMonth(2024, 2));   // leap
  EXPECT_EQ(28, daysInMonth(2026, 2));   // non-leap
  EXPECT_EQ(31, daysInMonth(2026, 1));
  EXPECT_EQ(30, daysInMonth(2026, 4));
}

TEST(TimeFormat, FormatClock24h) {
  char b[16];
  LocalTime t = applyTz(kEpoch, 0);        // 14:09
  formatClock(b, sizeof(b), t, false);
  EXPECT_STREQ("14:09", b);
}

TEST(TimeFormat, FormatClock12h) {
  char b[16];
  LocalTime t = applyTz(kEpoch, 0);        // 14:09 -> 2:09 PM
  formatClock(b, sizeof(b), t, true);
  EXPECT_STREQ("2:09 PM", b);
  LocalTime midnight = applyTz(kEpoch - 14 * 3600 - 9 * 60, 0);  // 00:00
  formatClock(b, sizeof(b), midnight, true);
  EXPECT_STREQ("12:00 AM", b);
}

TEST(TimeFormat, FormatDateVariants) {
  char b[24];
  LocalTime t = applyTz(kEpoch, 0);   // 2026-07-01
  formatDate(b, sizeof(b), t, DateFormat::DMY);   EXPECT_STREQ("01/07/2026", b);
  formatDate(b, sizeof(b), t, DateFormat::MDY);   EXPECT_STREQ("07/01/2026", b);
  formatDate(b, sizeof(b), t, DateFormat::DMonY); EXPECT_STREQ("1 Jul 2026", b);
}

TEST(TimeFormat, FormatShortDateHasWeekday) {
  char b[24];
  LocalTime t = applyTz(kEpoch, 0);
  formatShortDate(b, sizeof(b), t, DateFormat::DMonY); EXPECT_STREQ("Wed 1 Jul 2026", b);
  formatShortDate(b, sizeof(b), t, DateFormat::DMY);   EXPECT_STREQ("Wed 01/07/2026", b);
}

TEST(TimeFormat, FormatOffset) {
  char b[16];
  formatOffset(b, sizeof(b), 0);    EXPECT_STREQ("UTC", b);
  formatOffset(b, sizeof(b), 330);  EXPECT_STREQ("UTC+05:30", b);
  formatOffset(b, sizeof(b), -480); EXPECT_STREQ("UTC-08:00", b);
}

TEST(TimeFormat, FormatStamp) {
  char b[24];
  formatStamp(b, sizeof(b), kEpoch, kEpoch, 0, false);        EXPECT_STREQ("14:09", b);          // same day
  formatStamp(b, sizeof(b), kEpoch, kEpoch, 0, true);         EXPECT_STREQ("2:09 PM", b);        // 12h
  formatStamp(b, sizeof(b), kEpoch, kEpoch + 2*86400, 0, false); EXPECT_STREQ("1 Jul 14:09", b); // older day
  formatStamp(b, sizeof(b), kEpoch, 0, 0, false);            EXPECT_STREQ("14:09", b);           // unknown now -> time only
  b[0] = 'x';
  formatStamp(b, sizeof(b), 0, kEpoch, 0, false);            EXPECT_STREQ("", b);                // unknown time -> blank
}

#include <mishmesh/applets/SetTimeApplet.h>
#include <mishmesh/core/AppletHost.h>

namespace {
struct FakeApp : mishmesh::AppServices {
  uint32_t epoch = 0;
  int16_t  off = 0;
  uint32_t written = 0;
  bool     didWrite = false;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint8_t  datefmt = 0;
  uint32_t epochSeconds() const override { return epoch; }
  int16_t  tzOffsetMinutes() const override { return off; }
  void     setEpochSeconds(uint32_t s) override { written = s; didWrite = true; }
  uint8_t  dateFormat() const override { return datefmt; }
  void     setDateFormat(uint8_t f) override { datefmt = f; }
};
}  // namespace

using mishmesh::InputEvent;

TEST(SetTimeApplet, SeedsFromLocalTime) {
  FakeApp app; app.epoch = kEpoch; app.off = 330;   // 19:39 local
  auto& a = mishmesh::setTimeApplet();
  a.configure(&app);
  EXPECT_EQ(19, a.editing().hour);
  EXPECT_EQ(39, a.editing().minute);
  EXPECT_EQ(1,  a.editing().day);
}

TEST(SetTimeApplet, NavRightMovesField) {
  FakeApp app; app.epoch = kEpoch;
  auto& a = mishmesh::setTimeApplet();
  a.configure(&app);
  EXPECT_EQ(0, a.field());               // day
  a.onInput(InputEvent::NavRight);
  EXPECT_EQ(1, a.field());               // month
}

TEST(SetTimeApplet, NavUpIncrementsAndClampsDayToMonth) {
  FakeApp app; app.epoch = kEpoch; app.off = 0;   // 2026-07-01
  auto& a = mishmesh::setTimeApplet();
  a.configure(&app);
  a.onInput(InputEvent::NavRight);       // -> month
  a.onInput(InputEvent::NavUp);          // month 7 -> 8
  EXPECT_EQ(8, a.editing().month);
  // set day to 31 (Aug has 31 days: 1 + 30 steps, no wrap), then move to a
  // 30-day month -> clampDay must drop it to 30.
  a.onInput(InputEvent::NavLeft);        // -> day
  for (int i = 0; i < 30; i++) a.onInput(InputEvent::NavUp);  // 1 -> 31 in Aug
  EXPECT_EQ(31, a.editing().day);
  a.onInput(InputEvent::NavRight);       // -> month
  a.onInput(InputEvent::NavUp);          // Aug(8)->Sep(9), 30 days
  EXPECT_EQ(30, a.editing().day);        // clamped
}

TEST(SetTimeApplet, SelectCommitsUtc) {
  FakeApp app; app.epoch = kEpoch; app.off = 330;
  auto& a = mishmesh::setTimeApplet();
  a.configure(&app);
  a.onInput(InputEvent::Select);
  EXPECT_TRUE(app.didWrite);
  EXPECT_EQ(kEpoch, app.written);        // unchanged fields round-trip back to kEpoch
}

TEST(SetTimeApplet, SelectZeroesSeconds) {
  FakeApp app; app.epoch = kEpoch + 45; app.off = 330;   // 14:09:45 UTC
  auto& a = mishmesh::setTimeApplet();
  a.configure(&app);
  a.onInput(InputEvent::Select);
  EXPECT_EQ(kEpoch, app.written);        // seconds zeroed -> 14:09:00, not :45
}

#include <mishmesh/applets/settings/TimeSettingsPanel.h>

TEST(TimeSettingsPanel, TogglesFormatOnSelect) {
  FakeApp app; app.epoch = kEpoch;
  static bool s_fmt12 = false;
  struct FmtApp : FakeApp {
    bool* fmt;
    bool timeFormat12h() const override { return *fmt; }
    void setTimeFormat12h(bool on) override { *fmt = on; }
  } fa; fa.fmt = &s_fmt12; fa.epoch = kEpoch;

  mishmesh::AppletContext ctx; ctx.app = &fa;
  auto& p = mishmesh::timeSettings();
  p.begin(ctx);
  // Row order: TimeZone(0), Format(1), SetAuto(2), [SetTime(3) only when manual].
  // Move to Format and Select.
  p.onInput(mishmesh::InputEvent::NavDown);   // -> Format
  p.onInput(mishmesh::InputEvent::Select);
  EXPECT_TRUE(s_fmt12);
}

TEST(TimeSettingsPanel, SetTimeRowHiddenWhenAuto) {
  struct AutoApp : FakeApp {
    bool autoOn = true;
    bool autoTimeSync() const override { return autoOn; }
    void setAutoTimeSync(bool on) override { autoOn = on; }
  } aa; aa.epoch = kEpoch;

  mishmesh::AppletContext ctx; ctx.app = &aa;
  auto& p = mishmesh::timeSettings();
  p.begin(ctx);
  EXPECT_EQ(8, p.rowCountForTest());   // Zone/Fmt/DateFmt/AlarmSnd/AlarmVol/TimerSnd/TimerVol/SetAuto
  EXPECT_TRUE(p.autoToggleForTest());  // pill ON when auto is ON
  aa.autoOn = false;
  p.onShow();
  EXPECT_EQ(9, p.rowCountForTest());   // SetTime appears in manual mode
  EXPECT_FALSE(p.autoToggleForTest()); // pill OFF when auto is OFF
}

TEST(TimeSettingsPanel, PicksDateFormatViaModal) {
  FakeApp app; app.epoch = kEpoch; app.datefmt = 0;   // DMY
  mishmesh::AppletContext ctx; ctx.app = &app;
  auto& p = mishmesh::timeSettings();
  p.begin(ctx);
  // Row order: TimeZone(0), TimeFmt(1), DateFmt(2). Select opens a stepper modal.
  p.onInput(mishmesh::InputEvent::NavDown);
  p.onInput(mishmesh::InputEvent::NavDown);   // -> DateFmt row
  EXPECT_FALSE(p.modalActive());
  p.onInput(mishmesh::InputEvent::Select);    // open modal
  EXPECT_TRUE(p.modalActive());
  EXPECT_EQ(0, app.datefmt);                  // unchanged until confirmed
  p.onInput(mishmesh::InputEvent::NavRight);  // step DMY -> MDY
  p.onInput(mishmesh::InputEvent::Select);    // confirm
  EXPECT_FALSE(p.modalActive());
  EXPECT_EQ(1, app.datefmt);
}

TEST(SetTimeApplet, MdyOrderPutsMonthFirst) {
  FakeApp app; app.epoch = kEpoch; app.off = 0; app.datefmt = 1;   // MDY
  auto& a = mishmesh::setTimeApplet();
  a.configure(&app);
  // Display index 0 is Month under MDY; stepping it changes the month.
  EXPECT_EQ(7, a.editing().month);
  a.onInput(InputEvent::NavUp);   // field 0 = Month -> 8
  EXPECT_EQ(8, a.editing().month);
  EXPECT_EQ(1, a.editing().day);  // day untouched
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
