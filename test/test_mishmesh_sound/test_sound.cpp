#include <gtest/gtest.h>
#include <mishmesh/sound/RtttlSource.h>
#include <mishmesh/sound/ScoreSource.h>
#include <mishmesh/sound/NoteTable.h>
#include <mishmesh/sound/ToneSequencer.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>
#include <FakeToneOutput.h>
using namespace mishmesh::sound;

TEST(RtttlSource, ParsesNotesRestsAndDurations) {
  RtttlSource s;
  s.set("T:d=4,o=5,b=120:8c,8p,4a");   // wholenote = 60000/120*4 = 2000ms
  uint16_t f, d;

  ASSERT_TRUE(s.nextEvent(f, d));
  EXPECT_EQ(noteHz(0, 1), f);   // c5
  EXPECT_EQ(250u, d);           // 2000/8

  ASSERT_TRUE(s.nextEvent(f, d));
  EXPECT_EQ(0u, f);             // rest
  EXPECT_EQ(250u, d);

  ASSERT_TRUE(s.nextEvent(f, d));
  EXPECT_EQ(noteHz(9, 1), f);   // a5
  EXPECT_EQ(500u, d);           // 2000/4

  EXPECT_FALSE(s.nextEvent(f, d));
}

TEST(RtttlSource, ResetReplaysFromStart) {
  RtttlSource s;
  s.set("T:d=4,o=5,b=120:8c");
  uint16_t f, d;
  ASSERT_TRUE(s.nextEvent(f, d));
  EXPECT_FALSE(s.nextEvent(f, d));
  s.reset();
  ASSERT_TRUE(s.nextEvent(f, d));
  EXPECT_EQ(noteHz(0, 1), f);
}

// soundStart from the hopper game (game.cpp): rising C5 D5 E5 F5 G5, each 100ms
// (200ms on the last), separated by 25ms rests.
static const uint8_t kSoundStart[] = {
  0x90, 72, 0, 100, 0x80, 0, 25,
  0x90, 74, 0, 100, 0x80, 0, 25,
  0x90, 76, 0, 100, 0x80, 0, 25,
  0x90, 77, 0, 100, 0x80, 0, 25,
  0x90, 79, 0, 200, 0x80, 0xF0
};

static uint16_t midiHz(uint8_t m) { return noteHz(m % 12, m/12 - 5); }

TEST(ScoreSource, ParsesArduboyStartJingleOneToOne) {
  ScoreSource s;
  s.set(kSoundStart);
  uint16_t f, d;
  const uint8_t notes[] = {72, 74, 76, 77, 79};
  for (int i = 0; i < 5; i++) {
    ASSERT_TRUE(s.nextEvent(f, d));
    EXPECT_EQ(midiHz(notes[i]), f);
    EXPECT_EQ(i == 4 ? 200u : 100u, d);
    if (i < 4) {                       // rest between notes
      ASSERT_TRUE(s.nextEvent(f, d));
      EXPECT_EQ(0u, f);
      EXPECT_EQ(25u, d);
    }
  }
  EXPECT_FALSE(s.nextEvent(f, d));      // 0x80 then 0xF0 -> end
}

// Minimal hand-built source: two notes then end.
struct TwoNoteSource : ISoundSource {
  int i = 0;
  void reset() override { i = 0; }
  bool nextEvent(uint16_t& f, uint16_t& d) override {
    if (i == 0) { f = 440; d = 100; i++; return true; }
    if (i == 1) { f = 880; d = 50;  i++; return true; }
    return false;
  }
};

TEST(ToneSequencer, PlaysNotesInOrderThenStops) {
  FakeToneOutput out;
  TwoNoteSource src;
  ToneSequencer seq;
  seq.begin(&out);
  seq.start(&src, VolumeLevel::High, 1000);
  ASSERT_EQ(1u, out.calls.size());
  EXPECT_EQ(440, out.calls[0].freq);
  EXPECT_EQ(VolumeLevel::High, out.calls[0].vol);

  seq.tick(1050);                 // first note still playing (due at 1100)
  EXPECT_EQ(1u, out.calls.size());

  seq.tick(1100);                 // advance to second note
  ASSERT_EQ(2u, out.calls.size());
  EXPECT_EQ(880, out.calls[1].freq);
  EXPECT_TRUE(seq.isPlaying());

  seq.tick(1150);                 // end of stream -> silence + stop
  ASSERT_EQ(3u, out.calls.size());
  EXPECT_TRUE(out.calls[2].silence);
  EXPECT_FALSE(seq.isPlaying());
}

static const char* kBeep = "T:d=4,o=5,b=120:8c";

TEST(SoundEngine, PlaysWhenEnabledAndUnmuted) {
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  EXPECT_TRUE(eng.playRtttl(kBeep, SoundCategory::Notification));
  eng.tick(0);
  EXPECT_FALSE(out.calls.empty());
}

// Regression: the engine must initialize its tone-output backend. Without this,
// the device PWM driver's begin() never ran, _pwm stayed null, and every tone()
// no-op'd -> total silence on hardware (invisible to a no-op fake).
TEST(SoundEngine, BeginInitializesToneOutput) {
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  EXPECT_TRUE(out.begun);
}

TEST(SoundEngine, MasterMuteAndCategoryDisableSuppress) {
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  eng.setMasterMute(true);
  EXPECT_FALSE(eng.playRtttl(kBeep, SoundCategory::Notification));
  eng.setMasterMute(false);
  eng.setCategoryEnabled(SoundCategory::Ui, false);
  EXPECT_FALSE(eng.playRtttl(kBeep, SoundCategory::Ui));
  EXPECT_TRUE(eng.playRtttl(kBeep, SoundCategory::Notification));
}

TEST(SoundEngine, ExclusiveLockSuppressesNonGame) {
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  int gameOwner = 0;
  EXPECT_TRUE(eng.acquireExclusive(&gameOwner));
  EXPECT_TRUE(eng.acquireExclusive(&gameOwner));   // same owner re-acquire returns true
  EXPECT_FALSE(eng.playRtttl(kBeep, SoundCategory::Notification));   // suppressed
  static const uint8_t score[] = {0x90, 72, 0, 50, 0x80, 0xF0};
  EXPECT_TRUE(eng.playScore(score, SoundCategory::Game));            // allowed
  eng.releaseExclusive(&gameOwner);
  EXPECT_TRUE(eng.playRtttl(kBeep, SoundCategory::Notification));    // resumes
}

TEST(SoundEngine, PriorityPreemption) {
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  EXPECT_TRUE(eng.playRtttl(kBeep, SoundCategory::Notification));    // playing
  eng.tick(0);
  EXPECT_FALSE(eng.playRtttl(kBeep, SoundCategory::Ui));             // lower -> dropped
  EXPECT_TRUE(eng.playRtttl(kBeep, SoundCategory::System));          // higher -> wins
}

TEST(SoundEngine, CategoryMaskRoundTrips) {
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  eng.setCategoryEnabled(SoundCategory::Ui, false);
  uint8_t m = eng.categoryMask();
  SoundEngine eng2; eng2.begin(&out);
  eng2.setCategoryMask(m);
  EXPECT_FALSE(eng2.categoryEnabled(SoundCategory::Ui));
  EXPECT_TRUE(eng2.categoryEnabled(SoundCategory::Notification));
}

TEST(SoundEngine, EqualPriorityIsNotDropped) {
  using namespace mishmesh::sound;
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  static const uint8_t s[] = {0x90, 72, 0, 50, 0x80, 0xF0};
  EXPECT_TRUE(eng.playScore(s, SoundCategory::Game));   // queued (pending)
  EXPECT_TRUE(eng.playScore(s, SoundCategory::Game));   // same priority -> must NOT be dropped
}

TEST(Sounds, TableHasNamesAndCategories) {
  using namespace mishmesh::sound;
  const SoundDef* d = soundDef(SoundId::MsgChime);
  ASSERT_NE(nullptr, d);
  EXPECT_STREQ("Nokia", d->name);
  EXPECT_EQ(SoundCategory::Notification, d->category);
  EXPECT_EQ(nullptr, soundDef(SoundId::COUNT));
}

TEST(SoundEngine, PlaySoundIdRoutesByCategory) {
  using namespace mishmesh::sound;
  FakeToneOutput out;
  SoundEngine eng; eng.begin(&out);
  eng.setCategoryEnabled(SoundCategory::System, false);
  EXPECT_FALSE(eng.play(SoundId::BootJingle));     // System disabled
  EXPECT_TRUE(eng.play(SoundId::MsgChime));        // Notification enabled
}

TEST(SoundEngine, ActiveEngineAccessor) {
  using namespace mishmesh::sound;
  SoundEngine eng;
  setActiveEngine(&eng);
  EXPECT_EQ(&eng, activeEngine());
  setActiveEngine(nullptr);
  EXPECT_EQ(nullptr, activeEngine());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
