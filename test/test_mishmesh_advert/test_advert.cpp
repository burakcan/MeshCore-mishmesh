#include <gtest/gtest.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/AdvertApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/applets/DiscoverDetailApplet.h>
#include <mishmesh/applets/settings/AdvertSettingsPanel.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/NameValidation.h>
#include "FakeDisplayDriver.h"
#include "FakeContactsService.h"
#include <cstring>

using namespace mishmesh;

namespace {
// AppServices that records sendAdvert and supplies battery/clock for the tab UI.
class FakeAdvertApp : public AppServices {
public:
  int calls = 0; bool lastFlood = false; bool ret = true; uint32_t now = 0;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return now; }
  bool sendAdvert(bool flood) override { calls++; lastFlood = flood; return ret; }
};

// AppServices not overriding sendAdvert, to assert the safe default.
class BareApp : public AppServices {
public:
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
};

FakeContactsService::Row mkRow(const char* name, const char* key6, uint8_t type, uint32_t heardAt) {
  FakeContactsService::Row r; r.name = name; memcpy(r.pubkey, key6, 6); r.type = type; r.heardAt = heardAt;
  return r;
}

// AppServices with a settable name + share flag, for the settings panel.
class FakeNameApp : public AppServices {
public:
  char name[32] = "Old";
  bool share = false;
  const char* nodeName() const override { return name; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
  bool shareLocationInAdvert() const override { return share; }
  void setShareLocationInAdvert(bool on) override { share = on; }
  bool setNodeName(const char* n) override {
    if (!isValidNodeName(n)) return false;
    strncpy(name, n, sizeof(name) - 1); name[sizeof(name) - 1] = 0; return true;
  }
};
}  // namespace

TEST(AdvertSettingsPanel, ModelRowsAndValues) {
  FakeNameApp app;
  AppletContext ctx; ctx.app = &app;
  AdvertSettingsPanel& p = advertSettings();
  p.begin(ctx);
  EXPECT_EQ(2, p.rowCountForTest());
  EXPECT_STREQ("Device name",    p.labelForTest(0));
  EXPECT_STREQ("Share position", p.labelForTest(1));
  EXPECT_STREQ("Old",            p.valueForTest(0));
}

TEST(AdvertSettingsPanel, SharePositionRowStillToggles) {
  FakeNameApp app;
  AppletContext ctx; ctx.app = &app;
  AdvertSettingsPanel& p = advertSettings();
  p.begin(ctx);
  EXPECT_FALSE(app.share);
  EXPECT_TRUE(p.onInput(InputEvent::NavDown));   // -> row 1 (Share position)
  EXPECT_TRUE(p.onInput(InputEvent::Select));
  EXPECT_TRUE(app.share);
}

TEST(AdvertSettingsPanel, DeviceNameRowOpensEditorWithoutCrashing) {
  FakeNameApp app;
  AppletContext ctx; ctx.app = &app;             // no host -> push is skipped
  AdvertSettingsPanel& p = advertSettings();
  p.begin(ctx);
  EXPECT_TRUE(p.onInput(InputEvent::Select));    // row 0 = Device name
  EXPECT_STREQ("Old", app.name);                 // no change until keypad confirms
}

TEST(AppServicesAdvert, DefaultSendIsSafeNoop) {
  BareApp a;
  EXPECT_FALSE(a.sendAdvert(false));
  EXPECT_FALSE(a.sendAdvert(true));
}

// --- Advert (send) tab: unchanged behaviour under the tabbed structure ---

TEST(AdvertApplet, SendTabZeroHopThenFlood) {
  FakeAdvertApp app; FakeContactsService svc;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AdvertApplet applet; applet.onStart(ctx);

  EXPECT_TRUE(applet.onInput(InputEvent::Select));   // row 0 = zero hop
  EXPECT_EQ(1, app.calls); EXPECT_FALSE(app.lastFlood);

  EXPECT_TRUE(applet.onInput(InputEvent::NavDown));  // move to row 1 = flood
  EXPECT_TRUE(applet.onInput(InputEvent::Select));
  EXPECT_EQ(2, app.calls); EXPECT_TRUE(app.lastFlood);
}

TEST(AdvertApplet, SendModelLabelsAndIcons) {
  AdvertApplet::SendModel m;
  EXPECT_EQ(2, m.count());
  EXPECT_STREQ("Zero hop", m.label(0));
  EXPECT_STREQ("Flood routed", m.label(1));
  EXPECT_EQ((uint16_t)Icon::Wifi,  m.icon(0));
  EXPECT_EQ((uint16_t)Icon::Radio, m.icon(1));
}

TEST(AdvertApplet, BackBubbles) {
  FakeAdvertApp app; FakeContactsService svc;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AdvertApplet applet; applet.onStart(ctx);
  EXPECT_FALSE(applet.onInput(InputEvent::Back));   // not consumed -> host pops
  EXPECT_EQ(0, app.calls);
}

// --- Recent tab ---

TEST(AdvertApplet, RecentTabRoutesContactVsDiscover) {
  FakeAdvertApp app; app.now = 1000; FakeContactsService svc;
  // A known contact (also a recent advert) and a stranger (recent only).
  svc.chats.push_back(mkRow("Alice", "ALICE!", (uint8_t)ContactKind::Chat, 940));
  svc.recent.push_back(mkRow("Alice", "ALICE!", (uint8_t)ContactKind::Chat, 940));     // index 0: contact
  svc.recent.push_back(mkRow("Strange", "STRGR0", (uint8_t)ContactKind::Chat, 900));   // index 1: not a contact

  FakeDisplayDriver d;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  AdvertApplet applet; host.setRoot(&applet);

  host.dispatch(InputEvent::NavRight);   // switch to Recent tab (model reset -> row 0)
  host.dispatch(InputEvent::Select);     // row 0 (Alice) -> contact detail
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ((Applet*)&contactDetailApplet(), host.foreground());
  host.pop();                            // back to the AdvertApplet (Recent tab kept)

  host.dispatch(InputEvent::NavDown);    // row 1 (Strange)
  host.dispatch(InputEvent::Select);     // -> discover detail
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ((Applet*)&discoverDetailApplet(), host.foreground());
}

TEST(AdvertApplet, RecentModelLabelAndAge) {
  FakeAdvertApp app; app.now = 1000; FakeContactsService svc;
  svc.recent.push_back(mkRow("North", "NORTH0", (uint8_t)ContactKind::Repeater, 880));  // 120s ago
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AdvertApplet applet; applet.onStart(ctx);

  AdvertApplet::RecentModel& m = applet.recentModel();
  EXPECT_EQ(1, m.count());
  EXPECT_STREQ("4E North", m.label(0));         // repeater hex prefix ('N' == 0x4E)
  EXPECT_STREQ("2m", m.value(0));               // 1000 - 880 = 120s
  EXPECT_EQ((uint16_t)Icon::Radio, m.icon(0));  // repeater icon
}

TEST(AdvertApplet, RecentModelAgeBlankWhenClockUnknown) {
  FakeAdvertApp app; app.now = 0; FakeContactsService svc;
  svc.recent.push_back(mkRow("North", "NORTH0", (uint8_t)ContactKind::Chat, 880));
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AdvertApplet applet; applet.onStart(ctx);
  EXPECT_EQ(nullptr, applet.recentModel().value(0));
}

TEST(AdvertApplet, BothTabsRenderWithoutCrashing) {
  FakeAdvertApp app; app.now = 1000; FakeContactsService svc;
  svc.recent.push_back(mkRow("North", "NORTH0", (uint8_t)ContactKind::Repeater, 880));
  FakeDisplayDriver d;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  AdvertApplet applet; host.setRoot(&applet);
  host.loop(0);                          // Advert tab
  EXPECT_FALSE(d.fills.empty());
  host.dispatch(InputEvent::NavRight);   // Recent tab
  host.loop(0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
