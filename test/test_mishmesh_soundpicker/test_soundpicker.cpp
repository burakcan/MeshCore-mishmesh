#include <gtest/gtest.h>
#include <mishmesh/applets/SoundPickerApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/sound/Sounds.h>
#include "FakeMessagesService.h"
#include "FakeDisplayDriver.h"

using namespace mishmesh;
using namespace mishmesh::sound;

// Minimal AppServices stub exposing the per-type tone bytes.
struct FakeApp : AppServices {
  uint8_t ch = 0, dm = 0;
  const char* nodeName() const override { return "node"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  uint8_t notifyTone(bool channel) const override { return channel ? ch : dm; }
  void setNotifyTone(bool channel, uint8_t e) override { (channel ? ch : dm) = e; }
};

static AppletContext makeCtx(FakeApp& app, FakeMessagesService& svc, SoundEngine& snd) {
  AppletContext ctx;
  ctx.app = &app; ctx.messages = &svc; ctx.sound = &snd;
  return ctx;
}

TEST(SoundPicker, GlobalRowsAreTonesThenSilent) {
  FakeApp app; FakeMessagesService svc; SoundEngine snd;
  auto ctx = makeCtx(app, svc, snd);
  auto& p = soundPickerApplet();
  p.setGlobal(/*channel*/true, "Channel msgs");
  p.onStart(ctx);
  ListModel& m = p;
  EXPECT_EQ(m.count(), notifyToneCount() + 1);            // tones + Silent
  EXPECT_STREQ(m.label(0), notifyToneName(0));            // first tone
  EXPECT_STREQ(m.label(notifyToneCount()), "Silent");
}

TEST(SoundPicker, PerChatHasDefaultRowFirst) {
  FakeApp app; FakeMessagesService svc; SoundEngine snd;
  auto ctx = makeCtx(app, svc, snd);
  mishmesh::ConvoKey ch{}; ch.type = 1; ch.id[0] = 2;
  auto& p = soundPickerApplet();
  p.setChat(ch, "#chan2");
  p.onStart(ctx);
  ListModel& m = p;
  EXPECT_EQ(m.count(), notifyToneCount() + 2);            // Default + tones + Silent
  EXPECT_STREQ(m.label(0), "Default");
  EXPECT_STREQ(m.label(m.count() - 1), "Silent");
}

TEST(SoundPicker, RadioReflectsStoredGlobalByte) {
  FakeApp app; FakeMessagesService svc; SoundEngine snd;
  app.ch = NOTIFY_TONE_BASE + 2;                          // tone index 2 selected
  auto ctx = makeCtx(app, svc, snd);
  auto& p = soundPickerApplet();
  p.setGlobal(true, "Channel msgs");
  p.onStart(ctx);
  ListModel& m = p;
  EXPECT_TRUE(m.isRadio(2));
  EXPECT_TRUE(p.radioOn(2));                              // row 2 == tone index 2
  EXPECT_FALSE(p.radioOn(0));
}

TEST(SoundPicker, SelectWritesGlobalByte) {
  FakeApp app; FakeMessagesService svc; SoundEngine snd;
  auto ctx = makeCtx(app, svc, snd);
  auto& p = soundPickerApplet();
  p.setGlobal(false, "Direct msgs");                      // DM
  p.onStart(ctx);
  // move to Silent row and select
  for (int i = 0; i < notifyToneCount(); i++) p.onInput(InputEvent::NavDown);
  p.onInput(InputEvent::Select);
  EXPECT_EQ(app.dm, NOTIFY_TONE_SILENT);
}

TEST(SoundPicker, SelectWritesPerChatByte) {
  FakeApp app; FakeMessagesService svc; SoundEngine snd;
  mishmesh::ConvoKey ch{}; ch.type = 1; ch.id[0] = 5;
  auto ctx = makeCtx(app, svc, snd);
  auto& p = soundPickerApplet();
  p.setChat(ch, "#chan5");
  p.onStart(ctx);
  p.onInput(InputEvent::NavDown);                         // row 1 = first tone
  p.onInput(InputEvent::Select);
  EXPECT_EQ(svc.chatSound(ch), NOTIFY_TONE_BASE + 0);
}

TEST(SoundPicker, GlobalChannelDefaultByteHighlightsDroplet) {
  FakeApp app; FakeMessagesService svc; SoundEngine snd;   // app.ch defaults to 0
  auto ctx = makeCtx(app, svc, snd);
  auto& p = soundPickerApplet();
  p.setGlobal(/*channel*/true, "Channel msgs");
  p.onStart(ctx);
  // byte 0 for a channel = firmware default = Droplet = tone index 1
  int dropletRow = -1;
  for (int i = 0; i < notifyToneCount(); i++)
    if (notifyToneId(i) == notifyTypeDefault(true)) { dropletRow = i; break; }
  ASSERT_GE(dropletRow, 0);
  EXPECT_TRUE(p.radioOn(dropletRow));
  EXPECT_FALSE(p.radioOn(0));   // NOT Standard (index 0)
}

TEST(SoundPicker, SelectPerChatDefaultRowWritesDefault) {
  FakeApp app; FakeMessagesService svc; SoundEngine snd;
  mishmesh::ConvoKey ch{}; ch.type = 1; ch.id[0] = 7;
  svc.setChatSound(ch, NOTIFY_TONE_BASE + 2);   // start non-default
  auto ctx = makeCtx(app, svc, snd);
  auto& p = soundPickerApplet();
  p.setChat(ch, "#chan7");
  p.onStart(ctx);
  // After onStart (FIX2: cursor at rowForEncoded(NOTIFY_TONE_BASE+2) = 3 in per-chat mode).
  // Navigate up 3 rows to reach row 0 (Default).
  for (int i = 0; i < 3; i++) p.onInput(InputEvent::NavUp);
  p.onInput(InputEvent::Select);
  EXPECT_EQ(svc.chatSound(ch), NOTIFY_TONE_DEFAULT);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
