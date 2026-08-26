#include <gtest/gtest.h>
#include <mishmesh/applets/ChatNotifyApplet.h>
#include <mishmesh/applets/WakeOverrideApplet.h>
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
  EXPECT_EQ(m.count(), 5);                 // All / Mentions / Mute / Sound / Screen wake
  EXPECT_STREQ(m.label(3), "Sound");
  EXPECT_STREQ(m.label(4), "Screen wake");
  EXPECT_TRUE(m.isRadio(0));
  EXPECT_FALSE(m.isRadio(3));              // Sound row is not a radio
  EXPECT_FALSE(m.isRadio(4));              // Screen wake row is not a radio
}

TEST(ChatNotify, DmHasTwoLevelsPlusSound) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 0; k.id[0] = 0x10;
  auto& a = chatNotifyApplet();
  a.setTarget(k, "Bob");
  auto c = ctxFor(svc); a.onStart(c);
  ListModel& m = a;
  EXPECT_EQ(m.count(), 4);                 // All / Mute / Sound / Screen wake
  EXPECT_STREQ(m.label(2), "Sound");
  EXPECT_STREQ(m.label(3), "Screen wake");
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

TEST(ChatNotify, ScreenWakeRowValueAndDrill) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 0; k.id[0] = 0x30;   // DM: Screen wake is row 3
  auto& a = chatNotifyApplet();
  a.setTarget(k, "Bob");
  auto c = ctxFor(svc); a.onStart(c);
  ListModel& m = a;
  EXPECT_STREQ("Default", m.value(3));                   // default follows global
  svc.setChatWake(k, mishmesh::WakeOverride::Off);
  EXPECT_STREQ("Off", m.value(3));
}

TEST(WakeOverride, SelectingRowPersistsAndMarks) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 0; k.id[0] = 0x22;
  auto& a = mishmesh::wakeOverrideApplet();
  a.setTarget(k, "Bob");
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  a.onStart(ctx);
  mishmesh::ListModel& m = a;
  EXPECT_EQ(3, m.count());
  EXPECT_STREQ("Default", m.label(0));
  EXPECT_STREQ("Off", m.label(2));
  EXPECT_TRUE(m.radioOn(0));                       // starts at Default
  a.selectRowForTest(2);
  a.onInput(mishmesh::InputEvent::Select);         // pick Off
  EXPECT_EQ(mishmesh::WakeOverride::Off, svc.chatWake(k));
  EXPECT_TRUE(a.radioOn(2));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
