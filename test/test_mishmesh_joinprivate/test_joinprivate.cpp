#include <gtest/gtest.h>
#include <mishmesh/applets/JoinPrivateApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"
#include <string.h>

using namespace mishmesh;

namespace {
char g_name[32], g_key[33];
int  g_submitCalls = 0;
bool g_submitReturn = true;
bool keyValid(const char* s) {   // 32 hex chars
  int n = 0; for (const char* p = s; *p; ++p, ++n) {
    char c = *p; bool hex = (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');
    if (!hex) return false;
  }
  return n == 32;
}
bool submit(void*) { g_submitCalls++; return g_submitReturn; }
struct FakeApp : AppServices {
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
};
struct Root : Applet { Root() : Applet("root") {}
  int onRender(Canvas&) override { return 1000; }
  bool onInput(InputEvent) override { return false; } };
}  // namespace

TEST(JoinPrivateApplet, InvalidKeyBlocksSubmit) {
  g_submitCalls = 0;
  strcpy(g_name, "team"); strcpy(g_key, "nothex");
  FakeApp app; FakeDisplayDriver disp; AppletHost host(&disp, AppletContext{});
  Root root; host.setRoot(&root);
  AppletContext ctx; ctx.app = &app; ctx.host = &host;
  auto& w = joinPrivateApplet();
  w.configure(g_name, sizeof(g_name), g_key, sizeof(g_key), keyValid, submit, nullptr);
  host.push(&w);
  // rows: 0 Name, 1 Key, 2 Join
  host.dispatch(InputEvent::NavDown);
  host.dispatch(InputEvent::NavDown);   // focus Join
  host.dispatch(InputEvent::Select);    // invalid key -> no submit
  EXPECT_EQ(0, g_submitCalls);
}

TEST(JoinPrivateApplet, ValidSubmitPopsWhenTrue) {
  g_submitCalls = 0; g_submitReturn = true;
  strcpy(g_name, "team"); strcpy(g_key, "00112233445566778899aabbccddeeff");
  FakeApp app; FakeDisplayDriver disp; AppletHost host(&disp, AppletContext{});
  Root root; host.setRoot(&root);
  AppletContext ctx; ctx.app = &app; ctx.host = &host;
  auto& w = joinPrivateApplet();
  w.configure(g_name, sizeof(g_name), g_key, sizeof(g_key), keyValid, submit, nullptr);
  host.push(&w);
  int before = host.depthForTest();
  host.dispatch(InputEvent::NavDown);
  host.dispatch(InputEvent::NavDown);   // focus Join
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(1, g_submitCalls);
  EXPECT_LT(host.depthForTest(), before);   // popped on submit==true
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
