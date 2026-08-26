#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include <mishmesh/applets/DiscoverApplet.h>
#include <mishmesh/applets/DiscoverDetailApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
#include <cstring>

using namespace mishmesh;

TEST(DiscoverSeam, StartRecordsMaskAndScans) {
  FakeContactsService svc;
  EXPECT_FALSE(svc.discoverScanning());
  EXPECT_TRUE(svc.startNodeDiscover(0x04));
  EXPECT_EQ(1, svc.discoverStartCalls);
  EXPECT_EQ(0x04, svc.lastDiscoverMask);
  EXPECT_TRUE(svc.discoverScanning());
}

TEST(DiscoverSeam, ResultsEnumerateWithSeq) {
  FakeContactsService svc;
  EXPECT_EQ(0, svc.discoverResultCount());
  uint32_t before = svc.discoverSeq();
  svc.simulateDiscoverResult((const uint8_t*)"REPTR0", (uint8_t)ContactKind::Repeater, -28);
  EXPECT_EQ(1, svc.discoverResultCount());
  EXPECT_GT(svc.discoverSeq(), before);
  ContactsService::DiscoverResultView v;
  EXPECT_TRUE(svc.getDiscoverResult(0, v));
  EXPECT_EQ((uint8_t)ContactKind::Repeater, v.type);
  EXPECT_EQ(-28, v.snrX4);
  EXPECT_EQ('R', v.pubKey[0]);
  EXPECT_FALSE(svc.getDiscoverResult(1, v));
}

namespace {
class FakeApp : public AppServices {
public:
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
};
}  // namespace

TEST(DiscoverApplet, EmptyStateModel) {
  FakeApp app; FakeContactsService svc;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  DiscoverApplet applet; applet.onStart(ctx);
  DiscoverApplet::Model& m = applet.model();
  EXPECT_EQ(2, m.count());
  EXPECT_STREQ("Discover Repeaters", m.label(0));
  EXPECT_STREQ("Discover Sensors",   m.label(1));
  EXPECT_EQ((uint16_t)Icon::Radio, m.icon(0));
  EXPECT_EQ((uint16_t)Icon::Chip,  m.icon(1));
}

TEST(DiscoverApplet, RepeaterButtonFiresMask) {
  FakeApp app; FakeContactsService svc;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  DiscoverApplet applet; applet.onStart(ctx);
  EXPECT_TRUE(applet.onInput(InputEvent::Select));   // row 0
  EXPECT_EQ(1, svc.discoverStartCalls);
  EXPECT_EQ(0x04, svc.lastDiscoverMask);
}

TEST(DiscoverApplet, SensorButtonFiresMask) {
  FakeApp app; FakeContactsService svc;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  DiscoverApplet applet; applet.onStart(ctx);
  EXPECT_TRUE(applet.onInput(InputEvent::NavDown));  // row 1
  EXPECT_TRUE(applet.onInput(InputEvent::Select));
  EXPECT_EQ(1, svc.discoverStartCalls);
  EXPECT_EQ(0x10, svc.lastDiscoverMask);
}

TEST(DiscoverApplet, ScanningGatesButtons) {
  FakeApp app; FakeContactsService svc; svc.discoverScan = true;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  DiscoverApplet applet; applet.onStart(ctx);
  EXPECT_TRUE(applet.onInput(InputEvent::Select));   // consumed but ignored while scanning
  EXPECT_EQ(0, svc.discoverStartCalls);
}

TEST(DiscoverApplet, ResultRowLabelIconValue) {
  FakeApp app; FakeContactsService svc;
  svc.simulateDiscoverResult((const uint8_t*)"\xA1\xB2\xC3rep", (uint8_t)ContactKind::Repeater, -28);
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  DiscoverApplet applet; applet.onStart(ctx);
  DiscoverApplet::Model& m = applet.model();
  EXPECT_EQ(3, m.count());
  EXPECT_STREQ("A1B2C3", m.label(2));
  EXPECT_STREQ("-7 dB",  m.value(2));
  EXPECT_EQ((uint16_t)Icon::Radio, m.icon(2));
}

TEST(DiscoverApplet, ResultSelectOpensDetail) {
  FakeApp app; FakeContactsService svc;
  svc.simulateDiscoverResult((const uint8_t*)"REPTR0", (uint8_t)ContactKind::Repeater, -20);
  FakeDisplayDriver d;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  DiscoverApplet applet; host.setRoot(&applet);
  host.dispatch(InputEvent::NavDown);   // row 1
  host.dispatch(InputEvent::NavDown);   // row 2 = first result
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ((Applet*)&discoverDetailApplet(), host.foreground());
}

// A responder we already hold is not a discovery: routing it to the discover detail
// screen showed an added repeater as un-added, and its "Add to contacts" action then
// appended a duplicate (addContact() does not dedup by pubkey).
TEST(DiscoverApplet, ResultAlreadyAContactOpensContactDetail) {
  FakeApp app; FakeContactsService svc;
  FakeContactsService::Row r;
  r.name = "Hilltop";
  r.type = (uint8_t)ContactKind::Repeater;
  memcpy(r.pubkey, "REPTR0", 6);
  svc.repeaters.push_back(r);
  svc.simulateDiscoverResult((const uint8_t*)"REPTR0", (uint8_t)ContactKind::Repeater, -20);

  FakeDisplayDriver d;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  DiscoverApplet applet; host.setRoot(&applet);
  host.dispatch(InputEvent::NavDown);
  host.dispatch(InputEvent::NavDown);   // row 2 = first result
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ((Applet*)&contactDetailApplet(), host.foreground());
}

TEST(DiscoverApplet, RendersWithoutCrashing) {
  FakeApp app; FakeContactsService svc;
  FakeDisplayDriver d;
  AppletContext ctx; ctx.app = &app; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  DiscoverApplet applet; host.setRoot(&applet);
  host.loop(0);
  EXPECT_FALSE(d.fills.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
