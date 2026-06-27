#include <gtest/gtest.h>
#include <mishmesh/core/EmojiCatalog.h>

using namespace mishmesh;

static const uint32_t kCat[] = {0x1F600, 0x2764, 0x1F44D};

struct EmojiCatalogT : ::testing::Test {
  void TearDown() override { setEmojiCatalog(nullptr, 0); }
};

TEST_F(EmojiCatalogT, EmptyByDefault) {
  setEmojiCatalog(nullptr, 0);
  EXPECT_EQ(0, emojiCatalogCount());
  EXPECT_EQ(0u, emojiCatalogAt(0));
}

TEST_F(EmojiCatalogT, RegistersAndReads) {
  setEmojiCatalog(kCat, 3);
  EXPECT_EQ(3, emojiCatalogCount());
  EXPECT_EQ(0x1F600u, emojiCatalogAt(0));
  EXPECT_EQ(0x1F44Du, emojiCatalogAt(2));
  EXPECT_EQ(0u, emojiCatalogAt(3));      // out of range
}

TEST_F(EmojiCatalogT, NullPointerForcesEmpty) {
  setEmojiCatalog(nullptr, 5);           // count ignored when ptr null
  EXPECT_EQ(0, emojiCatalogCount());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
