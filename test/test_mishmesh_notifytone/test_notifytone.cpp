#include <gtest/gtest.h>
#include <mishmesh/sound/Sounds.h>

using namespace mishmesh::sound;

TEST(NotifyTone, ListHasSevenNamedTones) {
  EXPECT_EQ(notifyToneCount(), 7);
  // index 0/1 reuse the existing entries, renamed per spec
  EXPECT_STREQ(notifyToneName(0), "Standard");
  EXPECT_STREQ(notifyToneName(1), "Droplet");
  // every tone maps to a real SoundDef with a non-empty rtttl
  for (int i = 0; i < notifyToneCount(); i++) {
    const SoundDef* d = soundDef(notifyToneId(i));
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, SoundCategory::Notification);
    EXPECT_NE(d->rtttl, nullptr);
    EXPECT_GT((int)strlen(d->rtttl), 0);
    EXPECT_STREQ(notifyToneName(i), d->name);
  }
}

TEST(NotifyTone, TypeDefaultsMatchLegacyBehavior) {
  EXPECT_EQ(notifyTypeDefault(true),  SoundId::MsgKerplop);  // channel -> Droplet
  EXPECT_EQ(notifyTypeDefault(false), SoundId::MsgChime);    // DM -> Standard
}

TEST(NotifyTone, ResolverChatOverrideWinsThenType) {
  SoundId out = SoundId::UiTick;
  // chat=Default, type=Default -> firmware default for the type
  EXPECT_TRUE(resolveNotifyTone(0, 0, SoundId::MsgChime, out));
  EXPECT_EQ(out, SoundId::MsgChime);
  // chat picks tone index 2 (2+2 = encoded 4) -> notifyToneId(2)
  EXPECT_TRUE(resolveNotifyTone(NOTIFY_TONE_BASE + 2, 0, SoundId::MsgChime, out));
  EXPECT_EQ(out, notifyToneId(2));
  // chat=Default, type picks tone index 1 (Droplet)
  EXPECT_TRUE(resolveNotifyTone(0, NOTIFY_TONE_BASE + 1, SoundId::MsgChime, out));
  EXPECT_EQ(out, notifyToneId(1));
}

TEST(NotifyTone, ResolverSilent) {
  SoundId out = SoundId::UiTick;
  EXPECT_FALSE(resolveNotifyTone(NOTIFY_TONE_SILENT, 0, SoundId::MsgChime, out)); // chat Silent
  EXPECT_FALSE(resolveNotifyTone(0, NOTIFY_TONE_SILENT, SoundId::MsgChime, out)); // type Silent
}

TEST(NotifyTone, ResolverOutOfRangeFallsBackToDefault) {
  SoundId out = SoundId::UiTick;
  EXPECT_TRUE(resolveNotifyTone(NOTIFY_TONE_BASE + 99, 0, SoundId::MsgChime, out));
  EXPECT_EQ(out, SoundId::MsgChime);
}

TEST(NotifyTone, EncodedNameLabels) {
  EXPECT_STREQ(notifyToneEncodedName(0, /*perChat*/true,  SoundId::MsgChime), "Default");
  EXPECT_STREQ(notifyToneEncodedName(0, /*perChat*/false, SoundId::MsgChime), "Standard"); // type default's name
  EXPECT_STREQ(notifyToneEncodedName(NOTIFY_TONE_SILENT, true, SoundId::MsgChime), "Silent");
  EXPECT_STREQ(notifyToneEncodedName(NOTIFY_TONE_BASE + 1, false, SoundId::MsgChime), "Droplet");
}

#include "FakeMessagesService.h"

TEST(ChatSoundSeam, FakeRoundTrips) {
  FakeMessagesService svc;
  mishmesh::ConvoKey ch{}; ch.type = 1; ch.id[0] = 3;
  EXPECT_EQ(svc.chatSound(ch), 0u);                  // default
  svc.setChatSound(ch, mishmesh::sound::NOTIFY_TONE_BASE + 4);
  EXPECT_EQ(svc.chatSound(ch), mishmesh::sound::NOTIFY_TONE_BASE + 4);
  mishmesh::ConvoKey dm{}; dm.type = 0; dm.id[0] = 0xAB;
  EXPECT_EQ(svc.chatSound(dm), 0u);                  // distinct key untouched
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
