#include <gtest/gtest.h>
#include <mishmesh/core/ClockService.h>
#include <mishmesh/core/WorldClock.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/ClockApplet.h>
#include <mishmesh/applets/SoundPickerApplet.h>
#include <mishmesh/sound/Sounds.h>
#include "FakeDisplayDriver.h"

#include <map>
#include <string>
#include <vector>
#include <string.h>

using namespace mishmesh;

namespace {

struct FakeStorage : AppletStorage {
  std::map<std::string, std::vector<uint8_t>> kv;
  uint8_t load(const char* key, uint8_t* dst, uint8_t cap) override {
    auto it = kv.find(key);
    if (it == kv.end()) return 0;
    uint8_t n = (uint8_t)(it->second.size() < cap ? it->second.size() : cap);
    memcpy(dst, it->second.data(), n);
    return n;
  }
  bool save(const char* key, const uint8_t* src, uint8_t len) override {
    kv[key] = std::vector<uint8_t>(src, src + len);
    return true;
  }
};

uint32_t utc(int16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s = 0) {
  LocalTime lt{};
  lt.year = y; lt.month = mo; lt.day = d; lt.hour = h; lt.minute = mi; lt.second = s;
  return composeUtc(lt, 0);
}

int cityIndex(const char* name) {
  for (int i = 0; i < worldCityCount(); i++)
    if (strcmp(worldCity(i).name, name) == 0) return i;
  return -1;
}

}  // namespace

// ---- stopwatch --------------------------------------------------------------

TEST(Stopwatch, AccumulatesWhileRunning) {
  ClockService s;
  s.swToggle(1000);
  EXPECT_EQ(0u, s.swElapsedMs(1000));
  EXPECT_EQ(1500u, s.swElapsedMs(2500));
}

TEST(Stopwatch, FreezesWhenStoppedAndResumes) {
  ClockService s;
  s.swToggle(1000);
  s.swToggle(2500);                      // banked 1500
  EXPECT_EQ(1500u, s.swElapsedMs(9999));
  s.swToggle(3000);                      // resume
  EXPECT_EQ(2000u, s.swElapsedMs(3500));
}

TEST(Stopwatch, ResetClearsElapsedAndLaps) {
  ClockService s;
  s.swToggle(1000);
  s.swLap(2000);
  s.swReset();
  EXPECT_EQ(0u, s.swElapsedMs(5000));
  EXPECT_EQ(0, s.swLapCount());
}

TEST(Stopwatch, LapsNewestFirstWithRingCap) {
  ClockService s;
  s.swToggle(0);
  for (int i = 1; i <= 10; i++) s.swLap((uint32_t)i * 1000);
  EXPECT_EQ(ClockService::MAX_LAPS, s.swLapCount());
  EXPECT_EQ(10, s.swLapTotal());
  EXPECT_EQ(10, s.swLapNumber(0));       // newest kept
  EXPECT_EQ(10000u, s.swLapMs(0));
  EXPECT_EQ(3, s.swLapNumber(ClockService::MAX_LAPS - 1));   // oldest kept
  EXPECT_EQ(3000u, s.swLapMs(ClockService::MAX_LAPS - 1));
}

TEST(Stopwatch, LapIgnoredWhileStopped) {
  ClockService s;
  s.swLap(1000);
  EXPECT_EQ(0, s.swLapCount());
}

// ---- timer ------------------------------------------------------------------

TEST(Timer, CountsDownAndPauses) {
  ClockService s;
  s.tmSetDurationSecs(60);
  s.tmToggle(1000);                      // start
  EXPECT_EQ(30000u, s.tmRemainingMs(31000));
  s.tmToggle(31000);                     // pause
  EXPECT_EQ(30000u, s.tmRemainingMs(99999));
  s.tmToggle(50000);                     // resume
  EXPECT_EQ(20000u, s.tmRemainingMs(60000));
}

TEST(Timer, FiresOnceAndRearms) {
  ClockService s;
  s.tmSetDurationSecs(60);
  s.tmToggle(0);
  EXPECT_EQ(ClockEvent::None, s.tick(59999, 0, 0));
  EXPECT_EQ(ClockEvent::TimerDone, s.tick(60000, 0, 0));
  EXPECT_EQ(ClockEvent::TimerDone, s.ringing());
  EXPECT_FALSE(s.tmRunning());
  EXPECT_EQ(60000u, s.tmRemainingMs(70000));   // re-armed to full duration
  EXPECT_EQ(ClockEvent::None, s.tick(60050, 0, 0));
  s.acknowledge();
  EXPECT_EQ(ClockEvent::None, s.ringing());
}

TEST(Timer, DurationClampsAndIsIdleOnly) {
  ClockService s;
  s.tmSetDurationSecs(1);
  EXPECT_EQ(10u, s.tmDurationSecs());
  s.tmSetDurationSecs(999999);
  EXPECT_EQ(ClockService::TIMER_MAX_SECS, s.tmDurationSecs());
  s.tmSetDurationSecs(60);
  s.tmToggle(0);
  s.tmSetDurationSecs(120);              // ignored while running
  EXPECT_EQ(60u, s.tmDurationSecs());
}

// ---- alarm ------------------------------------------------------------------

TEST(Alarm, FiresOncePerMinuteInLocalTime) {
  ClockService s;
  s.setAlarm(7, 30, true, utc(2026, 7, 1, 12, 0), 120);
  // 07:30 local at UTC+2 is 05:30 UTC.
  uint32_t e = utc(2026, 7, 2, 5, 30);
  EXPECT_EQ(ClockEvent::None, s.tick(1000, e - 60, 120));   // 07:29
  EXPECT_EQ(ClockEvent::AlarmDue, s.tick(2000, e, 120));
  EXPECT_EQ(ClockEvent::None, s.tick(3000, e + 30, 120));   // same minute
  EXPECT_EQ(ClockEvent::AlarmDue, s.tick(4000, e + 86400, 120));   // next day
}

TEST(Alarm, RespectsTzOffset) {
  ClockService s;
  s.setAlarm(7, 30, true, 0, 0);
  uint32_t e = utc(2026, 7, 2, 5, 30);   // 07:30 only at UTC+2
  EXPECT_EQ(ClockEvent::None, s.tick(1000, e, 0));
  EXPECT_EQ(ClockEvent::AlarmDue, s.tick(2000, e, 120));
}

TEST(Alarm, DisabledOrClockUnsetNeverFires) {
  ClockService s;
  uint32_t e = utc(2026, 7, 2, 7, 30);
  s.setAlarm(7, 30, false, e - 3600, 0);
  EXPECT_EQ(ClockEvent::None, s.tick(1000, e, 0));
  s.setAlarm(7, 30, true, e - 3600, 0);
  EXPECT_EQ(ClockEvent::None, s.tick(2000, 0, 0));   // clock unset
}

TEST(Alarm, EnablingDuringTargetMinuteWaitsForNextDay) {
  ClockService s;
  uint32_t e = utc(2026, 7, 2, 7, 30, 20);
  s.setAlarm(7, 30, true, e, 0);         // set while 07:30 is already showing
  EXPECT_EQ(ClockEvent::None, s.tick(1000, e + 10, 0));
  EXPECT_EQ(ClockEvent::AlarmDue, s.tick(2000, e + 86400, 0));
}

TEST(Alarm, MinutesAway) {
  ClockService s;
  uint32_t e = utc(2026, 7, 2, 7, 0);
  s.setAlarm(7, 30, true, e, 0);
  EXPECT_EQ(30, s.alarmMinutesAway(e, 0));
  s.setAlarm(6, 30, true, e, 0);
  EXPECT_EQ(1410, s.alarmMinutesAway(e, 0));   // wraps to tomorrow
  s.setAlarm(6, 30, false, e, 0);
  EXPECT_EQ(-1, s.alarmMinutesAway(e, 0));
}

// ---- world clock ------------------------------------------------------------

TEST(WorldClock, FixedOffsetZones) {
  int delhi = cityIndex("Delhi");
  ASSERT_GE(delhi, 0);
  EXPECT_EQ(330, worldCityOffsetNow(delhi, utc(2026, 1, 15, 12, 0)));
  EXPECT_EQ(330, worldCityOffsetNow(delhi, utc(2026, 7, 15, 12, 0)));
}

TEST(WorldClock, EuDstBoundary) {
  int london = cityIndex("London");
  ASSERT_GE(london, 0);
  EXPECT_EQ(0, worldCityOffsetNow(london, utc(2026, 1, 15, 12, 0)));
  EXPECT_EQ(60, worldCityOffsetNow(london, utc(2026, 7, 15, 12, 0)));
  // 2026: last Sunday of March is the 29th; switch at 01:00 UTC.
  EXPECT_EQ(0, worldCityOffsetNow(london, utc(2026, 3, 29, 0, 59)));
  EXPECT_EQ(60, worldCityOffsetNow(london, utc(2026, 3, 29, 1, 1)));
  // Ends last Sunday of October (the 25th) at 01:00 UTC.
  EXPECT_EQ(60, worldCityOffsetNow(london, utc(2026, 10, 25, 0, 59)));
  EXPECT_EQ(0, worldCityOffsetNow(london, utc(2026, 10, 25, 1, 1)));
}

TEST(WorldClock, UsDstBoundary) {
  int ny = cityIndex("New York");
  ASSERT_GE(ny, 0);
  EXPECT_EQ(-300, worldCityOffsetNow(ny, utc(2026, 1, 15, 12, 0)));
  EXPECT_EQ(-240, worldCityOffsetNow(ny, utc(2026, 7, 15, 12, 0)));
  // 2026: 2nd Sunday of March is the 8th; 02:00 local standard = 07:00 UTC.
  EXPECT_EQ(-300, worldCityOffsetNow(ny, utc(2026, 3, 8, 6, 59)));
  EXPECT_EQ(-240, worldCityOffsetNow(ny, utc(2026, 3, 8, 7, 1)));
}

TEST(WorldClock, SouthernHemisphereSpansNewYear) {
  int syd = cityIndex("Sydney");
  ASSERT_GE(syd, 0);
  EXPECT_EQ(660, worldCityOffsetNow(syd, utc(2026, 1, 15, 12, 0)));   // summer = DST
  EXPECT_EQ(600, worldCityOffsetNow(syd, utc(2026, 7, 15, 12, 0)));   // winter
}

// ---- persistence ------------------------------------------------------------

TEST(ClockPersist, AlarmCitiesAndTimerDurationRoundTrip) {
  FakeStorage st;
  int berlin = cityIndex("Berlin"), tokyo = cityIndex("Tokyo");
  ASSERT_GE(berlin, 0); ASSERT_GE(tokyo, 0);
  {
    ClockService a;
    a.begin(&st);
    a.setAlarm(6, 45, true, 0, 0);
    EXPECT_TRUE(a.addCity((uint8_t)berlin));
    EXPECT_TRUE(a.addCity((uint8_t)tokyo));
    EXPECT_FALSE(a.addCity((uint8_t)tokyo));   // duplicate rejected
    a.tmSetDurationSecs(90);
  }
  ClockService b;
  b.begin(&st);
  EXPECT_TRUE(b.alarmEnabled());
  EXPECT_EQ(6, b.alarmHour());
  EXPECT_EQ(45, b.alarmMinute());
  ASSERT_EQ(2, b.cityCount());
  EXPECT_EQ(berlin, b.cityAt(0));
  EXPECT_EQ(tokyo, b.cityAt(1));
  EXPECT_EQ(90u, b.tmDurationSecs());

  b.removeCity(0);
  ClockService c;
  c.begin(&st);
  ASSERT_EQ(1, c.cityCount());
  EXPECT_EQ(tokyo, c.cityAt(0));
}

TEST(ClockPersist, RingToneAndVolumeRoundTrip) {
  FakeStorage st;
  {
    ClockService a;
    a.begin(&st);
    EXPECT_EQ(1, a.alarmToneIdx());   // defaults keep the original rings
    EXPECT_EQ(0, a.timerToneIdx());
    EXPECT_EQ(0, a.alarmVolume());    // default: follow the system volume
    a.setAlarmToneIdx(3);
    a.setTimerToneIdx(2);
    a.setAlarmVolume(3);              // High
    a.setTimerVolume(1);              // Low
  }
  ClockService b;
  b.begin(&st);
  EXPECT_EQ(3, b.alarmToneIdx());
  EXPECT_EQ(2, b.timerToneIdx());
  EXPECT_EQ(3, b.alarmVolume());
  EXPECT_EQ(1, b.timerVolume());
}

// ---- engine volume override ---------------------------------------------------

#include <mishmesh/sound/SoundEngine.h>
#include <FakeToneOutput.h>

TEST(RingVolume, OverridePlaysAtFixedLevelEvenWhenSystemMuted) {
  FakeToneOutput out;
  sound::SoundEngine eng;
  eng.begin(&out);
  eng.setVolume(sound::VolumeLevel::Mute);   // system volume dialed to Mute
  ASSERT_TRUE(eng.play(sound::SoundId::AlarmRing, sound::VolumeLevel::High));
  eng.tick(0);
  ASSERT_FALSE(out.calls.empty());
  EXPECT_EQ(sound::VolumeLevel::High, out.calls[0].vol);

  // A later normal play must not inherit the one-shot override.
  eng.stop();
  out.calls.clear();
  eng.setVolume(sound::VolumeLevel::Low);
  ASSERT_TRUE(eng.play(sound::SoundId::AlarmRing));
  eng.tick(1000);
  ASSERT_FALSE(out.calls.empty());
  EXPECT_EQ(sound::VolumeLevel::Low, out.calls[0].vol);
}

// ---- ring-tune list ----------------------------------------------------------

TEST(ClockTones, DefaultsMatchOriginalRingsAndClampOutOfRange) {
  using namespace mishmesh::sound;
  ASSERT_GE(clockToneCount(), 7);
  EXPECT_EQ(SoundId::TimerDone, clockToneId(0));   // stored default 0 = old timer ring
  EXPECT_EQ(SoundId::AlarmRing, clockToneId(1));   // stored default 1 = old alarm ring
  EXPECT_STREQ("Nokia", clockToneName(2));
  EXPECT_EQ(clockToneId(0), clockToneId(999));     // stale stored byte -> first tune
  for (int i = 0; i < clockToneCount(); i++) {     // every tune is a named System ring
    const SoundDef* d = soundDef(clockToneId(i));
    ASSERT_NE(nullptr, d);
    EXPECT_EQ(SoundCategory::System, d->category);
    EXPECT_NE(nullptr, strchr(d->rtttl, ':'));
  }
}

// ---- applet -----------------------------------------------------------------

namespace {
struct FakeApp : AppServices {
  bool fmt12 = false;
  const char* nodeName() const override { return "test"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool timeFormat12h() const override { return fmt12; }
};

struct ClockAppletFixture : ::testing::Test {
  FakeDisplayDriver d;
  ClockApplet app;
  FakeApp svc;
  AppletContext ctx;
  void SetUp() override {
    clockService().resetForTest();
    ctx.app = &svc;
    app.onStart(ctx);
  }
  void render() { Canvas c(&d, 1000); app.onRender(c); }
};
}

TEST_F(ClockAppletFixture, NavRightWalksAllFiveTabs) {
  EXPECT_EQ(0, app.selectedTabForTest());
  for (int i = 1; i <= 4; i++) {
    app.onInput(InputEvent::NavRight);
    EXPECT_EQ(i, app.selectedTabForTest());
  }
}

TEST_F(ClockAppletFixture, SelectTogglesStopwatchService) {
  app.onInput(InputEvent::Select);
  EXPECT_TRUE(clockService().swRunning());
  app.onInput(InputEvent::Select);
  EXPECT_FALSE(clockService().swRunning());
  app.onInput(InputEvent::SelectLong);
  EXPECT_EQ(0u, clockService().swElapsedMs(0));
}

TEST_F(ClockAppletFixture, TimerEditorSetsExactDuration) {
  app.onInput(InputEvent::NavRight);     // Timer tab
  app.onInput(InputEvent::NavUp);        // open the H:MM:SS editor
  EXPECT_TRUE(app.editingTimerForTest());
  // Seeded 0:05:00 with the cursor on minutes.
  app.onInput(InputEvent::NavDown);      // minutes 5 -> 4
  app.onInput(InputEvent::NavLeft);      // hours field
  app.onInput(InputEvent::NavUp);        // 0 -> 1
  app.onInput(InputEvent::NavRight);     // minutes
  app.onInput(InputEvent::NavRight);     // seconds
  app.onInput(InputEvent::NavUp);        // 0 -> 1
  app.onInput(InputEvent::Select);       // save
  EXPECT_FALSE(app.editingTimerForTest());
  EXPECT_EQ(3600u + 4 * 60u + 1u, clockService().tmDurationSecs());
}

TEST_F(ClockAppletFixture, TimerEditorCancelKeepsDuration) {
  app.onInput(InputEvent::NavRight);     // Timer tab
  app.onInput(InputEvent::NavDown);      // open editor (either direction)
  EXPECT_TRUE(app.editingTimerForTest());
  app.onInput(InputEvent::NavUp);
  app.onInput(InputEvent::Back);         // cancel
  EXPECT_FALSE(app.editingTimerForTest());
  EXPECT_EQ(300u, clockService().tmDurationSecs());
  app.onInput(InputEvent::Select);       // Select still starts the timer
  EXPECT_TRUE(clockService().tmRunning());
}

TEST_F(ClockAppletFixture, AlarmEditorSavesAndEnables) {
  app.onInput(InputEvent::NavRight);
  app.onInput(InputEvent::NavRight);     // Alarm tab
  app.onInput(InputEvent::Select);       // open editor on the Time row
  EXPECT_TRUE(app.editingAlarmForTest());
  app.onInput(InputEvent::NavUp);        // hour 7 -> 8
  app.onInput(InputEvent::NavRight);     // minute field
  app.onInput(InputEvent::NavDown);      // minute 0 -> 59
  app.onInput(InputEvent::Select);       // save
  EXPECT_FALSE(app.editingAlarmForTest());
  EXPECT_TRUE(clockService().alarmEnabled());
  EXPECT_EQ(8, clockService().alarmHour());
  EXPECT_EQ(59, clockService().alarmMinute());
}

TEST_F(ClockAppletFixture, AlarmEditorUsesAmPmIn12hMode) {
  svc.fmt12 = true;
  app.onInput(InputEvent::NavRight);
  app.onInput(InputEvent::NavRight);     // Alarm tab
  app.onInput(InputEvent::Select);       // editor seeded 7:00 AM, cursor on hour
  EXPECT_TRUE(app.editingAlarmForTest());
  app.onInput(InputEvent::NavLeft);      // wrap to the AM/PM field
  app.onInput(InputEvent::NavUp);        // AM -> PM
  app.onInput(InputEvent::Select);       // save
  EXPECT_EQ(19, clockService().alarmHour());   // 7 PM stored as 24h

  app.onInput(InputEvent::Select);       // reopen: seeded 7:00 PM
  app.onInput(InputEvent::NavUp);        // hour 7 -> 8
  app.onInput(InputEvent::Select);
  EXPECT_EQ(20, clockService().alarmHour());

  // 12 AM / 12 PM edge: step hour 8 PM up 4x -> 12 PM stays PM = 12h noon.
  app.onInput(InputEvent::Select);
  for (int i = 0; i < 4; i++) app.onInput(InputEvent::NavUp);   // 8..12
  render();                              // editor open: draws the AM/PM field path
  app.onInput(InputEvent::Select);
  EXPECT_EQ(12, clockService().alarmHour());   // 12 PM == 12:00
}

TEST_F(ClockAppletFixture, WorldTabAddsCityViaPicker) {
  for (int i = 0; i < 3; i++) app.onInput(InputEvent::NavRight);   // World tab
  app.onInput(InputEvent::Select);       // "Add city" row
  EXPECT_TRUE(app.pickingCityForTest());
  app.onInput(InputEvent::NavDown);
  app.onInput(InputEvent::NavDown);
  app.onInput(InputEvent::Select);
  EXPECT_FALSE(app.pickingCityForTest());
  ASSERT_EQ(1, clockService().cityCount());
  EXPECT_EQ(2, clockService().cityAt(0));
  app.onInput(InputEvent::SelectLong);   // remove it again
  EXPECT_EQ(0, clockService().cityCount());
}

TEST_F(ClockAppletFixture, SoundPickerClockModeSetsAlarmTone) {
  auto& p = soundPickerApplet();
  p.setClock(true, "Alarm sound");
  p.onStart(ctx);
  EXPECT_EQ(sound::clockToneCount(), p.count());
  EXPECT_STREQ("Beeper", p.label(1));
  EXPECT_TRUE(p.radioOn(1));             // opens on the current alarm tone
  p.onInput(InputEvent::NavDown);
  p.onInput(InputEvent::Select);         // pick row 2 = "Nokia"
  EXPECT_EQ(2, clockService().alarmToneIdx());
  EXPECT_TRUE(p.radioOn(2));
  EXPECT_EQ(0, clockService().timerToneIdx());   // timer untouched
}

TEST_F(ClockAppletFixture, RendersEveryTabWithoutServices) {
  for (int t = 0; t < 5; t++) {
    render();
    app.onInput(InputEvent::NavRight);
  }
  render();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
