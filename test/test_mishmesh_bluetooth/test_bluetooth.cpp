#include <gtest/gtest.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/BluetoothApplet.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
// Concrete AppServices that does NOT override the BLE virtuals, to assert defaults.
class BareApp : public AppServices {
public:
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
};
}  // namespace

TEST(AppServicesBle, DefaultsAreSafe) {
  BareApp a;
  EXPECT_FALSE(a.bleSupported());
  EXPECT_FALSE(a.bleEnabled());
  EXPECT_FALSE(a.bleConnected());
  EXPECT_EQ(0u, a.blePin());
  a.setBleEnabled(true);   // no-op must not crash
}

namespace {
// AppServices with settable BLE state; records setBleEnabled() calls.
class FakeBleApp : public AppServices {
public:
  bool enabled = false, connected = false, supported = true;
  uint32_t pin = 0;
  int setCalls = 0; bool lastSet = false;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool bleSupported() const override { return supported; }
  bool bleEnabled() const override { return enabled; }
  bool bleConnected() const override { return connected; }
  uint32_t blePin() const override { return pin; }
  void setBleEnabled(bool on) override { setCalls++; lastSet = on; enabled = on; }
};
}  // namespace

TEST(BluetoothApplet, StatusTextMapping) {
  // Status reflects the link only; the Toggle carries the radio on/off state.
  EXPECT_STREQ("Not connected", BluetoothApplet::statusText(false));
  EXPECT_STREQ("Connected",     BluetoothApplet::statusText(true));
}

TEST(BluetoothApplet, PinVisibility) {
  EXPECT_TRUE (BluetoothApplet::showPin(123456, false));  // have pin, not paired
  EXPECT_FALSE(BluetoothApplet::showPin(0,      false));  // no pin
  EXPECT_FALSE(BluetoothApplet::showPin(123456, true));   // paired -> irrelevant
}

TEST(BluetoothApplet, SelectTogglesAndConsumes) {
  FakeBleApp app; app.enabled = false;
  AppletContext ctx; ctx.app = &app;
  BluetoothApplet applet;
  applet.onStart(ctx);

  EXPECT_TRUE(applet.onInput(InputEvent::Select));   // consumed
  EXPECT_EQ(1, app.setCalls);
  EXPECT_TRUE(app.lastSet);                          // off -> on
  EXPECT_TRUE(applet.onInput(InputEvent::Select));
  EXPECT_EQ(2, app.setCalls);
  EXPECT_FALSE(app.lastSet);                         // on -> off
}

TEST(BluetoothApplet, BackBubbles) {
  FakeBleApp app;
  AppletContext ctx; ctx.app = &app;
  BluetoothApplet applet;
  applet.onStart(ctx);
  EXPECT_FALSE(applet.onInput(InputEvent::Back));    // not consumed -> host pops
  EXPECT_FALSE(applet.onInput(InputEvent::NavUp));   // nothing to scroll
}

TEST(BluetoothApplet, RendersWithoutCrashing) {
  FakeDisplayDriver d;
  FakeBleApp app; app.enabled = true; app.pin = 123456;
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);
  BluetoothApplet applet;
  host.push(&applet);
  host.loop(0);
  EXPECT_FALSE(d.fills.empty());   // icon + text drawn as pixel runs
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
