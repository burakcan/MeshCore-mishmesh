#include <gtest/gtest.h>
#include <mishmesh/applets/ChatNotifyApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/sound/Sounds.h>
#include "FakeMessagesService.h"

using namespace mishmesh;

static AppletContext ctxFor(FakeMessagesService& svc) {
  AppletContext c; c.messages = &svc; return c;
}

TEST(ChatNotify, ChannelHasThreeLevelsPlusSound) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 1; k.id[0] = 1;
  auto& a = chatNotifyApplet();
  a.setTarget(k, "#chan1");
  auto c = ctxFor(svc); a.onStart(c);
  ListModel& m = a;
  EXPECT_EQ(m.count(), 4);                 // All / Mentions / Mute / Sound
  EXPECT_STREQ(m.label(3), "Sound");
  EXPECT_TRUE(m.isRadio(0));
  EXPECT_FALSE(m.isRadio(3));              // Sound row is not a radio
}

TEST(ChatNotify, DmHasTwoLevelsPlusSound) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 0; k.id[0] = 0x10;
  auto& a = chatNotifyApplet();
  a.setTarget(k, "Bob");
  auto c = ctxFor(svc); a.onStart(c);
  ListModel& m = a;
  EXPECT_EQ(m.count(), 3);                 // All / Mute / Sound
  EXPECT_STREQ(m.label(2), "Sound");
}

TEST(ChatNotify, SelectingLevelPersists) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 1; k.id[0] = 1;
  auto& a = chatNotifyApplet();
  a.setTarget(k, "#chan1");
  auto c = ctxFor(svc); a.onStart(c);
  a.onInput(InputEvent::NavDown);          // -> row 1 Mentions only
  a.onInput(InputEvent::Select);
  EXPECT_EQ(svc.notifyLevel(k), NotifyLevel::MentionsOnly);
  EXPECT_TRUE(a.radioOn(1));
}

TEST(ChatNotify, SoundRowValueReflectsChatSound) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 1; k.id[0] = 1;
  svc.setChatSound(k, mishmesh::sound::NOTIFY_TONE_BASE + 1);   // Droplet
  auto& a = chatNotifyApplet();
  a.setTarget(k, "#chan1");
  auto c = ctxFor(svc); a.onStart(c);
  ListModel& m = a;
  EXPECT_STREQ(m.value(3), "Droplet");
  // default (unset) shows "Default"
  mishmesh::ConvoKey k2{}; k2.type = 1; k2.id[0] = 2;
  a.setTarget(k2, "#chan2");
  a.onStart(c);
  EXPECT_STREQ(m.value(3), "Default");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
