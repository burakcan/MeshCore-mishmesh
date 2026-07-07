#include <gtest/gtest.h>
#include <mishmesh/applets/SetPathApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
#include <stdio.h>
#include <string.h>

using namespace mishmesh;

namespace {
char g_path[160]; int g_hash;
int  g_submitCalls = 0;
void hsLabel(int v, char* out, uint16_t cap) { snprintf(out, cap, "%d-byte", v); }
void pathFmt(const char* buf, char* out, uint16_t cap) { snprintf(out, cap, "%s", buf[0] ? "set" : "flood"); }
bool submit(void*) { g_submitCalls++; return true; }
struct FakeApp : AppServices {
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
};
struct Root : Applet { Root() : Applet("root") {}
  int onRender(Canvas&) override { return 1000; }
  bool onInput(InputEvent) override { return false; } };
}  // namespace

TEST(SetPathApplet, HashStepperChangesValue) {
  g_hash = 1; g_path[0] = 0;
  FakeApp app; FakeDisplayDriver disp; AppletHost host(&disp, AppletContext{});
  Root root; host.setRoot(&root);
  AppletContext ctx; ctx.app = &app; ctx.host = &host;
  auto& w = setPathApplet();
  w.configure("Bob", g_path, sizeof(g_path), &g_hash, 1, 3, hsLabel, pathFmt, submit, nullptr);
  host.push(&w);
  // rows: 0 Hash size, 1 Path, 2 Save
  host.dispatch(InputEvent::Select);       // open stepper on Hash size
  host.dispatch(InputEvent::NavUp);        // 1 -> 2
  host.dispatch(InputEvent::Select);       // confirm
  EXPECT_EQ(2, g_hash);
}

TEST(SetPathApplet, SavePopsOnSubmit) {
  g_hash = 1; g_path[0] = 0; g_submitCalls = 0;
  FakeApp app; FakeDisplayDriver disp; AppletHost host(&disp, AppletContext{});
  Root root; host.setRoot(&root);
  AppletContext ctx; ctx.app = &app; ctx.host = &host;
  auto& w = setPathApplet();
  w.configure("Bob", g_path, sizeof(g_path), &g_hash, 1, 3, hsLabel, pathFmt, submit, nullptr);
  host.push(&w);
  int before = host.depthForTest();
  host.dispatch(InputEvent::NavDown);
  host.dispatch(InputEvent::NavDown);      // focus Save
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(1, g_submitCalls);
  EXPECT_LT(host.depthForTest(), before);
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
