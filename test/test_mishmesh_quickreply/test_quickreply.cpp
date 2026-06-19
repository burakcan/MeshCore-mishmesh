#include <gtest/gtest.h>
#include <mishmesh/core/QuickReplyStore.h>
#include <string.h>

using namespace mishmesh;

TEST(QuickReplyStore, AddAppendsInOrder) {
  QuickReplyStore s;
  EXPECT_TRUE(s.add("On my way"));
  EXPECT_TRUE(s.add("OK"));
  EXPECT_EQ(2, s.count());
  EXPECT_STREQ("On my way", s.text(0));
  EXPECT_STREQ("OK", s.text(1));
}

TEST(QuickReplyStore, AddRejectsBlankAndOverflow) {
  QuickReplyStore s;
  EXPECT_FALSE(s.add(nullptr));
  EXPECT_FALSE(s.add(""));
  for (int i = 0; i < QuickReplyStore::MAX_ITEMS; i++) EXPECT_TRUE(s.add("x"));
  EXPECT_EQ(QuickReplyStore::MAX_ITEMS, s.count());
  EXPECT_FALSE(s.add("one too many"));
}

TEST(QuickReplyStore, AddTrimsToMaxLen) {
  QuickReplyStore s;
  char longStr[128];
  memset(longStr, 'a', sizeof(longStr));
  longStr[127] = 0;
  EXPECT_TRUE(s.add(longStr));
  EXPECT_EQ((int)QuickReplyStore::MAX_LEN, (int)strlen(s.text(0)));
}

TEST(QuickReplyStore, SetOverwritesInPlace) {
  QuickReplyStore s;
  s.add("a"); s.add("b");
  EXPECT_TRUE(s.set(0, "changed"));
  EXPECT_STREQ("changed", s.text(0));
  EXPECT_STREQ("b", s.text(1));
  EXPECT_FALSE(s.set(5, "nope"));
  EXPECT_FALSE(s.set(0, ""));
}

TEST(QuickReplyStore, RemoveShiftsDown) {
  QuickReplyStore s;
  s.add("a"); s.add("b"); s.add("c");
  EXPECT_TRUE(s.remove(0));
  EXPECT_EQ(2, s.count());
  EXPECT_STREQ("b", s.text(0));
  EXPECT_STREQ("c", s.text(1));
  EXPECT_FALSE(s.remove(9));
}

TEST(QuickReplyStore, MoveReorders) {
  QuickReplyStore s;
  s.add("a"); s.add("b"); s.add("c");
  EXPECT_TRUE(s.move(2, 0));           // c to front
  EXPECT_STREQ("c", s.text(0));
  EXPECT_STREQ("a", s.text(1));
  EXPECT_STREQ("b", s.text(2));
  EXPECT_TRUE(s.move(0, 1));           // adjacent swap (grab-mode step)
  EXPECT_STREQ("a", s.text(0));
  EXPECT_STREQ("c", s.text(1));
  EXPECT_FALSE(s.move(0, 0));
  EXPECT_FALSE(s.move(0, 9));
}

#include <mishmesh/core/AppletStorage.h>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

namespace {
// Minimal in-memory AppletStorage (mirrors test_mishmesh_storage's FakeStorage).
class FakeStorage : public mishmesh::AppletStorage {
public:
  std::map<std::string, std::vector<uint8_t>> data;
  uint8_t load(const char* key, uint8_t* dst, uint8_t cap) override {
    auto it = data.find(key);
    if (it == data.end()) return 0;
    uint8_t n = (uint8_t)std::min<size_t>(it->second.size(), cap);
    for (uint8_t i = 0; i < n; i++) dst[i] = it->second[i];
    return n;
  }
  bool save(const char* key, const uint8_t* src, uint8_t len) override {
    data[key] = std::vector<uint8_t>(src, src + len);
    return true;
  }
};
}

TEST(QuickReplyStore, FreshStoreLoadsEmpty) {
  FakeStorage fs;
  QuickReplyStore s;
  s.begin(&fs);
  EXPECT_EQ(0, s.count());
}

TEST(QuickReplyStore, MutationsPersistAndReload) {
  FakeStorage fs;
  {
    QuickReplyStore s;
    s.begin(&fs);
    s.add("On my way");
    s.add("OK");
    s.add("Call me");
  }
  QuickReplyStore reloaded;
  reloaded.begin(&fs);
  EXPECT_EQ(3, reloaded.count());
  EXPECT_STREQ("On my way", reloaded.text(0));
  EXPECT_STREQ("OK", reloaded.text(1));
  EXPECT_STREQ("Call me", reloaded.text(2));
}

TEST(QuickReplyStore, RemovePersistsShrunkList) {
  FakeStorage fs;
  {
    QuickReplyStore s;
    s.begin(&fs);
    s.add("a"); s.add("b"); s.add("c");
    s.remove(1);
  }
  QuickReplyStore reloaded;
  reloaded.begin(&fs);
  EXPECT_EQ(2, reloaded.count());
  EXPECT_STREQ("a", reloaded.text(0));
  EXPECT_STREQ("c", reloaded.text(1));
}

TEST(QuickReplyStore, DeletingAllStaysEmptyOnReload) {
  FakeStorage fs;
  {
    QuickReplyStore s;
    s.begin(&fs);
    s.add("only");
    s.remove(0);
  }
  QuickReplyStore reloaded;
  reloaded.begin(&fs);
  EXPECT_EQ(0, reloaded.count());   // cleared markers => not re-seeded, not stale
}

// move() reorders in memory only; a full flash rewrite per drag step would be too
// slow, so the order is committed once via save() (called by the panel on drop).
TEST(QuickReplyStore, MoveDoesNotPersistUntilSave) {
  FakeStorage fs;
  { QuickReplyStore s; s.begin(&fs); s.add("a"); s.add("b"); s.add("c");
    s.move(2, 0); }                     // reorder to c,a,b in memory - NOT persisted
  QuickReplyStore r1; r1.begin(&fs);
  EXPECT_STREQ("a", r1.text(0));        // disk still holds the add() order
  EXPECT_STREQ("b", r1.text(1));
  EXPECT_STREQ("c", r1.text(2));

  { QuickReplyStore s; s.begin(&fs); s.move(2, 0); s.save(); }   // commit the reorder
  QuickReplyStore r2; r2.begin(&fs);
  EXPECT_STREQ("c", r2.text(0));
  EXPECT_STREQ("a", r2.text(1));
  EXPECT_STREQ("b", r2.text(2));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
