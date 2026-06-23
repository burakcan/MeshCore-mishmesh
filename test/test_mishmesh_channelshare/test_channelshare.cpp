#include <gtest/gtest.h>
#include <mishmesh/applets/ChannelShareApplet.h>
#include <mishmesh/widgets/QrView.h>
#include <mishmesh/core/Canvas.h>
#include "FakeMessagesService.h"
#include "FakeDisplayDriver.h"
#include <cstring>

using namespace mishmesh;

// Minimal AppServices: ChannelShareApplet only reads battery (for the status bar).
struct FakeApp : AppServices {
  const char* nodeName() const override { return "me"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
};

static ConvoKey chan(uint8_t idx) { ConvoKey k{}; k.type = 1; k.id[0] = idx; return k; }

static AppletContext ctxFor(FakeApp& app, FakeMessagesService& svc) {
  AppletContext c; c.app = &app; c.messages = &svc; return c;
}

// --- QrView (vendored generator) ---

TEST(QrView, EncodesShortText) {
  QrView qr;
  ASSERT_TRUE(qr.build("HELLO"));
  EXPECT_TRUE(qr.valid());
  // size = 4*version + 17, so (size - 17) is a positive multiple of 4.
  int s = qr.moduleCount();
  EXPECT_GE(s, 21);
  EXPECT_EQ((s - 17) % 4, 0);
}

TEST(QrView, EncodesFullChannelUri) {
  QrView qr;
  ASSERT_TRUE(qr.build("meshcore://channel/add?name=My%20Chan&secret=8b3387e9c5cdea6ac9e5edbaa115cd72"));
  EXPECT_TRUE(qr.valid());
}

TEST(QrView, EmptyIsInvalid) {
  QrView qr;
  EXPECT_FALSE(qr.build(""));
  EXPECT_FALSE(qr.valid());
}

// --- ChannelShareApplet ---

TEST(ChannelShare, BuildsJoinUriWithEncodedName) {
  FakeApp app; FakeMessagesService svc;
  auto& a = channelShareApplet();
  a.setTarget(chan(1), "My Chan");
  auto c = ctxFor(app, svc); a.onStart(c);
  EXPECT_STREQ(a.keyHexForTest(), "8b3387e9c5cdea6ac9e5edbaa115cd72");
  EXPECT_STREQ(a.uriForTest(),
               "meshcore://channel/add?name=My%20Chan&secret=8b3387e9c5cdea6ac9e5edbaa115cd72");
}

TEST(ChannelShare, EncodesHashtagName) {
  FakeApp app; FakeMessagesService svc;
  auto& a = channelShareApplet();
  a.setTarget(chan(2), "#dev");
  auto c = ctxFor(app, svc); a.onStart(c);
  EXPECT_STREQ(a.uriForTest(),
               "meshcore://channel/add?name=%23dev&secret=8b3387e9c5cdea6ac9e5edbaa115cd72");
}

TEST(ChannelShare, UsesPerChannelKey) {
  FakeApp app; FakeMessagesService svc;
  svc.channelKeys["#chan3"] = "00112233445566778899aabbccddeeff";
  auto& a = channelShareApplet();
  a.setTarget(chan(3), "Chan");
  auto c = ctxFor(app, svc); a.onStart(c);
  EXPECT_STREQ(a.keyHexForTest(), "00112233445566778899aabbccddeeff");
}

TEST(ChannelShare, SelectTogglesQrAndKey) {
  FakeApp app; FakeMessagesService svc;
  auto& a = channelShareApplet();
  a.setTarget(chan(1), "Chan");
  auto c = ctxFor(app, svc); a.onStart(c);
  EXPECT_FALSE(a.showingKeyForTest());          // starts on the QR
  EXPECT_TRUE(a.onInput(InputEvent::Select));   // consumed
  EXPECT_TRUE(a.showingKeyForTest());           // now the key
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(a.showingKeyForTest());          // back to QR
}

TEST(ChannelShare, BackBubbles) {
  FakeApp app; FakeMessagesService svc;
  auto& a = channelShareApplet();
  a.setTarget(chan(1), "Chan");
  auto c = ctxFor(app, svc); a.onStart(c);
  EXPECT_FALSE(a.onInput(InputEvent::Back));    // not consumed -> host pops
}

TEST(ChannelShare, RendersBothModesWithoutCrash) {
  FakeApp app; FakeMessagesService svc;
  auto& a = channelShareApplet();
  a.setTarget(chan(1), "My Chan");
  auto c = ctxFor(app, svc); a.onStart(c);
  FakeDisplayDriver d; Canvas cv(&d);
  a.onRender(cv);                                // QR mode
  a.onInput(InputEvent::Select);
  a.onRender(cv);                                // key mode
  SUCCEED();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
