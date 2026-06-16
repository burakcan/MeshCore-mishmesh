#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/applets/settings/SystemInfoPanel.h>
#include "FakeDisplayDriver.h"

#include <string>

using namespace mishmesh;

namespace {

// Minimal AppServices with a settable SystemStats payload.
class FakeApp : public AppServices {
public:
  SystemStats stats;
  bool ok = true;
  const char* nodeName() const override { return "node"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool systemStats(SystemStats& out) const override {
    if (!ok) return false;
    out = stats;
    return true;
  }
};

SystemStats sampleStats() {
  SystemStats s;
  s.heapFreeBytes    = 145720;   // 142.3K
  s.heapMinFreeBytes = 141312;   // 138.0K
  s.heapTotalBytes   = 0;        // unknown -> "--", line omitted? no: Heap total only shown if known
  s.contactsUsed     = 12;
  s.contactsMax      = 100;
  s.storageUsedKb    = 48;
  s.storageTotalKb   = 192;
  s.uptimeSecs       = 4980;     // 1h 23m
  s.batteryMv        = 4010;     // 4.01V
  s.firmwareVersion  = "mishmesh-dev";
  return s;
}

std::string lineAt(char lines[][SYSSTATS_LINE_LEN], int i) { return std::string(lines[i]); }

}  // namespace

TEST(FormatSystemStats, RendersKnownFields) {
  char lines[SYSSTATS_MAX_LINES][SYSSTATS_LINE_LEN];
  int n = formatSystemStats(sampleStats(), lines, SYSSTATS_MAX_LINES);

  // heapTotal unknown -> no "Heap total" line.
  ASSERT_EQ(7, n);
  EXPECT_EQ("Heap free: 142.3K", lineAt(lines, 0));
  EXPECT_EQ("Heap min: 138.0K",  lineAt(lines, 1));
  EXPECT_EQ("Contacts: 12/100", lineAt(lines, 2));
  EXPECT_EQ("Storage: 48/192K", lineAt(lines, 3));
  EXPECT_EQ("Battery: 4.01V",   lineAt(lines, 4));
  EXPECT_EQ("Uptime: 1h 23m",   lineAt(lines, 5));
  EXPECT_EQ("FW: mishmesh-dev", lineAt(lines, 6));
}

TEST(FormatSystemStats, UnknownsRenderAsDashes) {
  SystemStats s;            // all zero / nullptr
  s.contactsMax = 100;
  char lines[SYSSTATS_MAX_LINES][SYSSTATS_LINE_LEN];
  int n = formatSystemStats(s, lines, SYSSTATS_MAX_LINES);

  // No Heap total (0), no FW (nullptr): Heap free, Heap min, Contacts, Storage, Battery, Uptime.
  ASSERT_EQ(6, n);
  EXPECT_EQ("Heap free: --",   lineAt(lines, 0));
  EXPECT_EQ("Heap min: --",    lineAt(lines, 1));
  EXPECT_EQ("Contacts: 0/100",lineAt(lines, 2));
  EXPECT_EQ("Storage: --",    lineAt(lines, 3));
  EXPECT_EQ("Battery: --", lineAt(lines, 4));
  EXPECT_EQ("Uptime: 0h 00m", lineAt(lines, 5));
}

TEST(FormatSystemStats, ShowsRamTotalWhenKnown) {
  SystemStats s = sampleStats();
  s.heapTotalBytes = 253952;   // 248.0K
  char lines[SYSSTATS_MAX_LINES][SYSSTATS_LINE_LEN];
  int n = formatSystemStats(s, lines, SYSSTATS_MAX_LINES);
  ASSERT_EQ(8, n);
  EXPECT_EQ("Heap total: 248.0K", lineAt(lines, 2));
}

TEST(SystemInfoPanel, ScrollsAndBubblesBack) {
  FakeApp app; app.stats = sampleStats();
  AppletContext ctx; ctx.app = &app;
  SystemInfoPanel panel; panel.begin(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  panel.renderBody(c, 0, 0, 128, 64);
  EXPECT_TRUE(panel.onInput(InputEvent::NavDown));   // scroll consumed
  EXPECT_FALSE(panel.onInput(InputEvent::Back));     // bubbles
}

TEST(SystemInfoPanel, RendersWithoutCrashing) {
  FakeApp app; app.stats = sampleStats();
  AppletContext ctx; ctx.app = &app;
  SystemInfoPanel panel; panel.begin(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  panel.renderBody(c, 0, 0, 128, 64);
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(SystemInfoPanel, UnavailableWhenServiceReturnsFalse) {
  FakeApp app; app.ok = false;
  AppletContext ctx; ctx.app = &app;
  SystemInfoPanel panel; panel.begin(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  panel.renderBody(c, 0, 0, 128, 64);
  EXPECT_STREQ("Stats unavailable", panel.lineForTest(0));   // test seam added in Step 3 header
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
