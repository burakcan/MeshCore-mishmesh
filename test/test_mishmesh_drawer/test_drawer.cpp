#include <gtest/gtest.h>
#include <mishmesh/core/Actions.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/widgets/QuickDrawer.h>
#include <mishmesh/sound/SoundEngine.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {

struct FakeApp : AppServices {
  bool ble = false, bleConn = false, bleSup = true;
  bool gpsSup = true, gps = false;
  uint8_t vol = 2;                       // Mid
  int adverts = 0; bool lastFlood = false; bool advertOk = true;
  sound::SoundEngine* eng = nullptr;

  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool bleSupported() const override { return bleSup; }
  bool bleEnabled() const override { return ble; }
  bool bleConnected() const override { return bleConn; }
  void setBleEnabled(bool on) override { ble = on; }
  bool gpsSupported() const override { return gpsSup; }
  bool gpsEnabled() const override { return gps; }
  void setGpsEnabled(bool on) override { gps = on; }
  void setSoundVolume(uint8_t v) override {
    vol = v;
    if (eng) eng->setVolume((sound::VolumeLevel)v);
  }
  bool sendAdvert(bool flood) override {
    adverts++; lastFlood = flood; return advertOk;
  }
};

// Drives draw() until animations settle so isOpen()/geometry are stable.
void settle(QuickDrawer& dr, Canvas& c) {
  for (int i = 0; i < 20 && dr.animating(); i++) dr.draw(c);
}

}  // namespace

TEST(QuickDrawer, OpensNavigatesAndCloses) {
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  QuickDrawer dr;
  dr.bind(ctx);
  EXPECT_FALSE(dr.isOpen());

  dr.open();
  EXPECT_TRUE(dr.isOpen());
  settle(dr, c);
  EXPECT_EQ(0, dr.selected());
  EXPECT_TRUE(dr.onInput(InputEvent::NavRight));
  EXPECT_EQ(1, dr.selected());
  EXPECT_TRUE(dr.onInput(InputEvent::NavLeft));
  EXPECT_TRUE(dr.onInput(InputEvent::NavLeft));   // wraps to last tile
  EXPECT_EQ(4, dr.selected());

  EXPECT_TRUE(dr.onInput(InputEvent::Back));      // starts close
  settle(dr, c);
  EXPECT_FALSE(dr.isOpen());
}

TEST(QuickDrawer, TogglesBleAndGps) {
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  QuickDrawer dr; dr.bind(ctx);
  dr.open(); settle(dr, c);

  EXPECT_TRUE(dr.onInput(InputEvent::Select));    // tile 0 = BLE
  EXPECT_TRUE(app.ble);

  dr.onInput(InputEvent::NavRight);
  dr.onInput(InputEvent::NavRight);               // tile 2 = GPS
  EXPECT_TRUE(dr.onInput(InputEvent::Select));
  EXPECT_TRUE(app.gps);
}

TEST(QuickDrawer, SoundTileCyclesVolume) {
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app;
  sound::SoundEngine eng;                          // host-safe; default Mid
  app.eng = &eng;
  AppletContext ctx; ctx.app = &app; ctx.sound = &eng;
  QuickDrawer dr; dr.bind(ctx);
  dr.open(); settle(dr, c);
  dr.onInput(InputEvent::NavRight);                // tile 1 = Sound

  dr.onInput(InputEvent::Select);                  // Mid -> High
  EXPECT_EQ(3, app.vol);
  dr.onInput(InputEvent::Select);                  // High -> Mute (wraps)
  EXPECT_EQ(0, app.vol);
  dr.onInput(InputEvent::Select);                  // Mute -> Low
  EXPECT_EQ(1, app.vol);
}

TEST(QuickDrawer, AdvertTileFiresZeroHopAndFlood) {
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  QuickDrawer dr; dr.bind(ctx);
  dr.open(); settle(dr, c);
  for (int i = 0; i < 3; i++) dr.onInput(InputEvent::NavRight);   // tile 3

  dr.onInput(InputEvent::Select);
  EXPECT_EQ(1, app.adverts);
  EXPECT_FALSE(app.lastFlood);
  dr.onInput(InputEvent::SelectLong);
  EXPECT_EQ(2, app.adverts);
  EXPECT_TRUE(app.lastFlood);
}

TEST(QuickDrawer, GpsUnsupportedDoesNotToggle) {
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app; app.gpsSup = false;
  AppletContext ctx; ctx.app = &app;
  QuickDrawer dr; dr.bind(ctx);
  dr.open(); settle(dr, c);
  dr.onInput(InputEvent::NavRight);
  dr.onInput(InputEvent::NavRight);                // tile 2 = GPS
  dr.onInput(InputEvent::Select);
  EXPECT_FALSE(app.gps);                            // stayed off ("No GPS")
}

TEST(QuickDrawer, CloseNowResetsWithoutAnimation) {
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  QuickDrawer dr; dr.bind(ctx);
  dr.open(); settle(dr, c);
  dr.onInput(InputEvent::NavRight);
  dr.closeNow();
  EXPECT_FALSE(dr.isOpen());
  EXPECT_FALSE(dr.animating());
  dr.open(); settle(dr, c);
  EXPECT_EQ(0, dr.selected());                      // selection reset
}

TEST(QuickDrawer, HidesBleTileWhenUnsupported) {
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app;
  app.bleSup = false;                               // e.g. a USB/serial build
  AppletContext ctx; ctx.app = &app;
  QuickDrawer dr; dr.bind(ctx);
  dr.open(); settle(dr, c);
  EXPECT_EQ(4, dr.tileCount());
  EXPECT_EQ((int)QuickDrawer::Sound, dr.selectedTile());   // first visible tile
  dr.onInput(InputEvent::NavLeft);                  // wraps within the 4 tiles
  EXPECT_EQ((int)QuickDrawer::Theme, dr.selectedTile());
  dr.onInput(InputEvent::NavRight);
  EXPECT_EQ((int)QuickDrawer::Sound, dr.selectedTile());
}

TEST(Actions, CycleSoundVolumePersistsViaApp) {
  FakeApp app;
  sound::SoundEngine eng;                           // default Mid
  app.eng = &eng;
  EXPECT_EQ(3, cycleSoundVolume(&app, &eng, nullptr));   // Mid -> High
  EXPECT_EQ(3, app.vol);                            // persisted through the app seam
  EXPECT_EQ(0, cycleSoundVolume(&app, &eng, nullptr));   // High -> Mute (wraps)
  EXPECT_EQ(1, cycleSoundVolume(nullptr, &eng, nullptr)); // no app: live-only change
  EXPECT_EQ(sound::VolumeLevel::Low, eng.volume());
}

TEST(Actions, ToggleBleFlipsState) {
  FakeApp app;
  EXPECT_FALSE(app.ble);
  EXPECT_TRUE(toggleBle(&app, nullptr));            // null host: no toast, still flips
  EXPECT_TRUE(app.ble);
  EXPECT_FALSE(toggleBle(&app, nullptr));
  EXPECT_FALSE(app.ble);
}

TEST(QuickDrawer, ThemeTileTogglesDarkMode) {
  uiPrefs().resetForTest();                         // dark by default
  FakeDisplayDriver d; Canvas c(&d);
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  QuickDrawer dr; dr.bind(ctx);
  dr.open(); settle(dr, c);
  dr.onInput(InputEvent::NavLeft);                  // wrap to the Theme tile
  EXPECT_EQ((int)QuickDrawer::Theme, dr.selectedTile());
  EXPECT_TRUE(uiPrefs().darkMode());
  dr.onInput(InputEvent::Select);
  EXPECT_FALSE(uiPrefs().darkMode());
  dr.onInput(InputEvent::Select);
  EXPECT_TRUE(uiPrefs().darkMode());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
