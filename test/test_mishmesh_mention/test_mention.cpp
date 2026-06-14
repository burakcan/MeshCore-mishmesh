#include <gtest/gtest.h>
#include <mishmesh/core/Mention.h>
using namespace mishmesh;

TEST(Mention, MatchesCaseInsensitive) {
  EXPECT_TRUE(textMentions("hey @burak ok", "Burak"));
  EXPECT_TRUE(textMentions("@Burak hi", "burak"));
}
TEST(Mention, RequiresWordBoundaryAfterName) {
  EXPECT_FALSE(textMentions("@burakk hi", "burak"));   // trailing alnum defeats it
  EXPECT_TRUE(textMentions("@burak, hi", "burak"));     // punctuation is a boundary
  EXPECT_TRUE(textMentions("ping @burak", "burak"));    // end of string is a boundary
}
TEST(Mention, RequiresAtSign) {
  EXPECT_FALSE(textMentions("burak hi", "burak"));
}
TEST(Mention, BroadcastTokensAreNotMentions) {
  EXPECT_FALSE(textMentions("@everyone hi", "burak"));
  EXPECT_FALSE(textMentions("@all hi", "burak"));
}
TEST(Mention, MatchesBracketedVariant) {
  EXPECT_TRUE(textMentions("hey @[Burak] ok", "burak"));   // the app's @[name] format
  EXPECT_TRUE(textMentions("@[burak]", "Burak"));           // case-insensitive, closes
  EXPECT_TRUE(textMentions("@[John Doe] hi", "John Doe"));  // brackets allow spaces
  EXPECT_FALSE(textMentions("@[burak", "burak"));           // unclosed -> no match
  EXPECT_FALSE(textMentions("@[burakk]", "burak"));         // wrong name inside brackets
}
TEST(Mention, EmptyOrNullIsNoMatch) {
  EXPECT_FALSE(textMentions("@ hi", ""));
  EXPECT_FALSE(textMentions(nullptr, "burak"));
  EXPECT_FALSE(textMentions("@burak", nullptr));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
