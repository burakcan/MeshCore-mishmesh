#include <gtest/gtest.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/settings/BluetoothPanel.h>
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

TEST(BluetoothPanel, StatusTextMapping) {
  // Status reflects the link only; the Toggle carries the radio on/off state.
  EXPECT_STREQ("Not connected", BluetoothPanel::statusText(false));
  EXPECT_STREQ("Connected",     BluetoothPanel::statusText(true));
}

TEST(BluetoothPanel, PinVisibility) {
  EXPECT_TRUE (BluetoothPanel::showPin(123456, false));  // have pin, not paired
  EXPECT_FALSE(BluetoothPanel::showPin(0,      false));  // no pin
  EXPECT_FALSE(BluetoothPanel::showPin(123456, true));   // paired -> irrelevant
}

TEST(BluetoothPanel, SelectTogglesAndConsumes) {
  FakeBleApp app;                       // existing fake with settable BLE state
  AppletContext ctx; ctx.app = &app;
  BluetoothPanel panel; panel.begin(ctx);
  bool before = app.bleEnabled();
  EXPECT_TRUE(panel.onInput(InputEvent::Select));   // toggle row
  EXPECT_NE(before, app.bleEnabled());
}

TEST(BluetoothPanel, BackBubbles) {
  FakeBleApp app;
  AppletContext ctx; ctx.app = &app;
  BluetoothPanel panel; panel.begin(ctx);
  EXPECT_FALSE(panel.onInput(InputEvent::Back));
}

TEST(BluetoothPanel, RendersWithoutCrashing) {
  FakeBleApp app;
  AppletContext ctx; ctx.app = &app;
  BluetoothPanel panel; panel.begin(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  panel.renderBody(c, 0, 0, 128, 64);
  EXPECT_GT(d.fills.size(), 0u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
