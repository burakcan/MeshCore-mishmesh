#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/SleepScreen.h>
#include <mishmesh/applets/onboarding_logo.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/applets/settings/DisplaySettingsPanel.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {

struct FakeApp : AppServices {
  uint32_t epoch = 0;
  bool faceSupported = false;
  uint8_t face = 0;
  const char* nodeName() const override { return "alice"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return epoch; }
  uint8_t orient = 0;
  bool sleepScreenSupported() const override { return faceSupported; }
  uint8_t sleepOrientation() const override { return orient; }
  void setSleepOrientation(uint8_t v) override { orient = v; }
  uint8_t sleepScreenIndex() const override { return face; }
  void setSleepScreenIndex(uint8_t idx) override { face = idx; }
};

struct StubApplet : Applet {
  StubApplet() : Applet("stub") {}
  int onRender(Canvas&) override { return 1000; }
};

// Sleeps the host: auto-off is 1s, so one quiet loop past that blanks the panel.
void sleepHost(AppletHost& host, uint32_t& now) {
  host.setAutoOffMillis(1000);
  host.loop(now);          // stamps the activity baseline
  now += 2000;
  host.loop(now);
}

int setBitsIn(const uint8_t* bits, int w, int h) {
  const int stride = (w + 7) / 8;
  int n = 0;
  for (int by = 0; by < h; by++)
    for (int bx = 0; bx < w; bx++)
      if (bits[by * stride + (bx >> 3)] & (0x80 >> (bx & 7))) n++;
  return n;
}

AppletContext ctxFor(FakeApp& app) {
  AppletContext ctx;
  ctx.app = &app;
  return ctx;
}

}  // namespace

TEST(SleepScreen, TableIsBoundedAndLabelled) {
  for (int i = 0; i < SLEEP_SCREEN_COUNT; i++) {
    ASSERT_NE(nullptr, sleepScreenAt(i));
    EXPECT_NE(nullptr, sleepScreenAt(i)->draw);
    EXPECT_STRNE("", sleepScreenLabel(i));
  }
  EXPECT_EQ(nullptr, sleepScreenAt(-1));
  EXPECT_EQ(nullptr, sleepScreenAt(SLEEP_SCREEN_COUNT));
}

TEST(SleepScreen, IndexZeroIsScreenOff) {
  EXPECT_STREQ("Screen off", sleepScreenLabel(0));
}

TEST(SleepScreen, StaticFacesNeverAskForAnotherRefresh) {
  FakeDisplayDriver d(250, 122);
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  Canvas c(&d);
  for (int i = 0; i < SLEEP_SCREEN_COUNT; i++) {
    const SleepScreen* f = sleepScreenAt(i);
    if (__builtin_strcmp(f->label, "Clock") == 0) continue;
    EXPECT_EQ(SLEEP_NEVER, f->draw(c, ctx)) << "face " << f->label;
  }
}

TEST(SleepScreen, ClockWakesOnTheMinuteBoundary) {
  FakeDisplayDriver d(250, 122);
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  Canvas c(&d);
  const SleepScreen* clock = nullptr;
  for (int i = 0; i < SLEEP_SCREEN_COUNT; i++)
    if (__builtin_strcmp(sleepScreenAt(i)->label, "Clock") == 0) clock = sleepScreenAt(i);
  ASSERT_NE(nullptr, clock);

  app.epoch = 1000 * 60;            // exactly on a minute
  EXPECT_EQ(60000, clock->draw(c, ctx));
  app.epoch = 1000 * 60 + 20;       // 20s in, 40s to go
  EXPECT_EQ(40000, clock->draw(c, ctx));
  app.epoch = 1000 * 60 + 59;
  EXPECT_EQ(1000, clock->draw(c, ctx));
}

TEST(SleepScreen, ClockWithNoTimeStillTicksButPaintsAnIdenticalFrame) {
  FakeDisplayDriver d(250, 122);
  FakeApp app;                       // epoch 0 = clock never set
  AppletContext ctx = ctxFor(app);
  Canvas c(&d);
  const SleepScreen* clock = nullptr;
  for (int i = 0; i < SLEEP_SCREEN_COUNT; i++)
    if (__builtin_strcmp(sleepScreenAt(i)->label, "Clock") == 0) clock = sleepScreenAt(i);
  ASSERT_NE(nullptr, clock);
  EXPECT_EQ(60000, clock->draw(c, ctx));
}

TEST(AppletHostSleepFace, EinkPaintsTheFaceOnceOnSleep) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(1);            // Logo: static

  uint32_t now = 1000;
  sleepHost(host, now);
  EXPECT_FALSE(d.on);
  EXPECT_EQ(1, host.sleepPaintsForTest());
}

TEST(AppletHostSleepFace, StaticFaceNeverRepaintsWhileAsleep) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(1);            // Logo

  uint32_t now = 1000;
  sleepHost(host, now);
  for (int i = 0; i < 600; i++) { now += 10000; host.loop(now); }   // ~100 min
  EXPECT_EQ(1, host.sleepPaintsForTest());
}

TEST(AppletHostSleepFace, ClockFaceRepaintsOncePerMinute) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  FakeApp app;
  app.epoch = 3600;                  // on a minute boundary
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(2);            // Clock

  uint32_t now = 1000;
  sleepHost(host, now);
  EXPECT_EQ(1, host.sleepPaintsForTest());

  for (int i = 0; i < 60; i++) { now += 1000; host.loop(now); }     // one minute of loops
  EXPECT_EQ(2, host.sleepPaintsForTest());
  for (int i = 0; i < 60; i++) { now += 1000; host.loop(now); }
  EXPECT_EQ(3, host.sleepPaintsForTest());
}

TEST(AppletHostSleepFace, WakeStopsSleepPainting) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  FakeApp app;
  app.epoch = 3600;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(2);

  uint32_t now = 1000;
  sleepHost(host, now);
  int painted = host.sleepPaintsForTest();

  host.wakeDisplay();
  host.setAutoOffMillis(0);          // stay awake, or it just falls asleep again
  EXPECT_TRUE(d.on);
  for (int i = 0; i < 300; i++) { now += 1000; host.loop(now); }
  EXPECT_EQ(painted, host.sleepPaintsForTest());
}

TEST(AppletHostSleepFace, NonEinkPanelKeepsTodaysBlankAndOff) {
  FakeDisplayDriver d(128, 64);      // OLED: turnOff really powers it down
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(2);            // even if a face is somehow configured

  uint32_t now = 1000;
  sleepHost(host, now);
  EXPECT_FALSE(d.on);
  EXPECT_EQ(0, host.sleepPaintsForTest());
  for (int i = 0; i < 600; i++) { now += 10000; host.loop(now); }
  EXPECT_EQ(0, host.sleepPaintsForTest());
}

TEST(AppletHostSleepFace, GhostClearingFullRefreshIsAtMostHourly) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  FakeApp app;
  app.epoch = 3600;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(2);            // Clock: 1440 paints a day

  uint32_t now = 1000;
  sleepHost(host, now);
  // Four hours of sleeping, stepping a minute at a time.
  for (int i = 0; i < 4 * 60 * 60; i++) { now += 1000; host.loop(now); }
  EXPECT_GE(host.sleepPaintsForTest(), 200);
  EXPECT_LE(d.fullRefreshes, 5);
  EXPECT_GE(d.fullRefreshes, 3);
}

TEST(AppletHostSleepFace, StaticFaceNeverAsksForAFullRefresh) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(1);            // Logo: nothing repaints, so nothing ghosts

  uint32_t now = 1000;
  sleepHost(host, now);
  for (int i = 0; i < 4 * 60 * 60; i++) { now += 1000; host.loop(now); }
  EXPECT_EQ(0, d.fullRefreshes);
}

TEST(AppletHostSleepFace, LogoFaceDrawsInTheForegroundColour) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  d.lightBackground = true;          // as the real e-ink panel reports itself
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setSleepScreen(1);            // Logo

  uint32_t now = 1000;
  sleepHost(host, now);
  EXPECT_EQ(2, d.xbms);              // MeshCore wordmark + mishmesh wordmark
  // The face paints straight after the background fill, so an XBM that inherits
  // whatever setColor last left comes out invisible - the whole bug.
  EXPECT_NE(themedColor(DisplayDriver::DARK), d.lastXbmColor);
  EXPECT_EQ(themedColor(DisplayDriver::LIGHT), d.lastXbmColor);
}

// A portrait panel with the UI turned to match, as the e-ink board reports itself.
AppletHost* portraitHost(FakeDisplayDriver& d, AppletContext& ctx, int uiRotation) {
  d.eink = true;
  d.orientable = true;
  d.rotation = uiRotation;
  AppletHost* h = new AppletHost(&d, ctx);
  h->setUiRotation(uiRotation);
  return h;
}

TEST(AppletHostSleepFace, LandscapeOnlyFaceTurnsThePanelAndBackOnWake) {
  FakeDisplayDriver d(122, 250);
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost* host = portraitHost(d, ctx, 1);   // UI in portrait
  StubApplet root;
  host->setRoot(&root);
  host->setSleepScreen(1);           // Logo: reads landscape only

  uint32_t now = 1000;
  sleepHost(*host, now);
  EXPECT_EQ(0, d.rotation);          // turned for the face rather than cropped
  EXPECT_EQ(1, host->sleepPaintsForTest());

  host->wakeDisplay();
  EXPECT_EQ(1, d.rotation);          // and put back the way the user had it
  delete host;
}

TEST(AppletHostSleepFace, ForcedOrientationKeepsTheFlipTheDeviceIsMountedWith) {
  FakeDisplayDriver d(122, 250);
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost* host = portraitHost(d, ctx, 3);   // portrait, upside down
  StubApplet root;
  host->setRoot(&root);
  host->setSleepScreen(1);

  uint32_t now = 1000;
  sleepHost(*host, now);
  EXPECT_EQ(2, d.rotation);          // landscape flipped, not landscape
  delete host;
}

TEST(AppletHostSleepFace, EitherFaceFollowsTheScreenOnAutoAndObeysAChoice) {
  FakeDisplayDriver d(122, 250);
  FakeApp app;
  app.epoch = 3600;
  AppletContext ctx = ctxFor(app);
  AppletHost* host = portraitHost(d, ctx, 1);
  StubApplet root;
  host->setRoot(&root);
  host->setSleepScreen(2);           // Clock: reads either way

  uint32_t now = 1000;
  sleepHost(*host, now);
  EXPECT_EQ(1, d.rotation);          // Auto: left as the user set it
  host->wakeDisplay();

  host->setSleepOrientation(1);      // Landscape
  now += 1000;
  sleepHost(*host, now);
  EXPECT_EQ(0, d.rotation);
  host->wakeDisplay();
  EXPECT_EQ(1, d.rotation);

  host->setSleepOrientation(3);      // Landscape flipped: absolute, not inherited
  now += 1000;
  sleepHost(*host, now);
  EXPECT_EQ(2, d.rotation);
  host->wakeDisplay();
  EXPECT_EQ(1, d.rotation);
  delete host;
}

TEST(AppletHostSleepFace, NoFaceLeavesTheRotationAlone) {
  FakeDisplayDriver d(122, 250);
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost* host = portraitHost(d, ctx, 1);
  StubApplet root;
  host->setRoot(&root);
  host->setSleepScreen(0);           // Screen off: nothing to orient
  host->setSleepOrientation(1);      // ignored - the face declares None

  uint32_t now = 1000;
  sleepHost(*host, now);
  EXPECT_EQ(1, d.rotation);
  delete host;
}

TEST(AppletHostSleepFace, RequestSleepBlanksOnTheNextPass) {
  FakeDisplayDriver d(250, 122);
  d.eink = true;
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  StubApplet root;
  host.setRoot(&root);
  host.setAutoOffMillis(0);          // no timer: the request is the only trigger
  host.setSleepScreen(1);

  host.loop(1000);
  EXPECT_TRUE(d.on);
  host.requestSleep();
  EXPECT_TRUE(d.on);                 // not until the in-flight frame has flushed
  host.loop(1100);
  EXPECT_FALSE(d.on);
  EXPECT_EQ(1, host.sleepPaintsForTest());
}

TEST(DisplaySettings, RowVisibilityFollowsTheDriverWithoutReopening) {
  FakeDisplayDriver d(250, 122);
  d.orientable = true;
  d.uiScalable = true;
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  ctx.host = &host;

  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);
  displaySettings().begin(ctx);
  EXPECT_STREQ("Interface size", displaySettings().rowLabelForTest(0));

  // Turning the panel to portrait withdraws the magnification choice. The list
  // has to lose the row now, not the next time the panel is opened.
  d.uiScalable = false;
  EXPECT_STRNE("Interface size", displaySettings().rowLabelForTest(0));
}

TEST(DisplaySettings, SleepOrientationRowOnlyForAFaceThatReadsEitherWay) {
  FakeDisplayDriver d(250, 122);
  d.orientable = true;
  FakeApp app;
  app.faceSupported = true;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  ctx.host = &host;

  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);

  app.face = 1;                      // Logo: landscape only, applied for it
  displaySettings().begin(ctx);
  for (int i = 0; i < 8; i++)
    EXPECT_STRNE("Sleep orientation", displaySettings().rowLabelForTest(i));

  app.face = 2;                      // Clock: reads either way, so ask
  displaySettings().begin(ctx);
  int row = -1;
  for (int i = 0; i < 8; i++)
    if (__builtin_strcmp(displaySettings().rowLabelForTest(i), "Sleep orientation") == 0) row = i;
  ASSERT_NE(-1, row);
  EXPECT_STREQ("Auto", displaySettings().rowValueForTest(row));
}

TEST(DisplaySettings, SleepScreenRowOnlyOnPanelsThatCanHoldAFace) {
  FakeDisplayDriver d(250, 122);
  FakeApp app;
  AppletContext ctx = ctxFor(app);
  AppletHost host(&d, ctx);
  ctx.host = &host;

  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);

  displaySettings().begin(ctx);
  for (int i = 0; i < 8; i++)
    EXPECT_STRNE("Sleep screen", displaySettings().rowLabelForTest(i));

  app.faceSupported = true;
  app.face = 2;
  displaySettings().begin(ctx);
  int row = -1;
  for (int i = 0; i < 8; i++)
    if (__builtin_strcmp(displaySettings().rowLabelForTest(i), "Sleep screen") == 0) row = i;
  ASSERT_NE(-1, row);
  EXPECT_STREQ("Clock", displaySettings().rowValueForTest(row));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
