#include <gtest/gtest.h>
#include <mishmesh/core/ContactFormat.h>
#include <mishmesh/text/Fonts.h>
#include <cstring>

using namespace mishmesh;

TEST(ContactFormat, KindIconMapping) {
  EXPECT_EQ((uint16_t)Icon::User,    kindIcon(ContactKind::Chat));
  EXPECT_EQ((uint16_t)Icon::Radio,   kindIcon(ContactKind::Repeater));
  EXPECT_EQ((uint16_t)Icon::Comment, kindIcon(ContactKind::Room));
  EXPECT_EQ((uint16_t)Icon::Chip,    kindIcon(ContactKind::Sensor));
}

TEST(ContactFormat, ContactLabelRepeaterPrefix) {
  uint8_t key[PUBKEY_LEN] = {0xAB, 0x01, 0x02};
  ContactView v{}; v.name = "North"; v.type = (uint8_t)ContactKind::Repeater; v.pubKey = key;
  char buf[44];
  EXPECT_STREQ("AB North", contactLabel(v, buf, sizeof(buf)));
}

TEST(ContactFormat, ContactLabelPlainName) {
  uint8_t key[PUBKEY_LEN] = {0xAB};
  ContactView v{}; v.name = "Alice"; v.type = (uint8_t)ContactKind::Chat; v.pubKey = key;
  char buf[44];
  EXPECT_STREQ("Alice", contactLabel(v, buf, sizeof(buf)));
}

TEST(ContactFormat, AgeFormatting) {   // guards the existing helper too
  char buf[12];
  contactFormatAge(45, buf, sizeof(buf));    EXPECT_STREQ("45s", buf);
  contactFormatAge(120, buf, sizeof(buf));   EXPECT_STREQ("2m", buf);
  contactFormatAge(7200, buf, sizeof(buf));  EXPECT_STREQ("2h", buf);
  contactFormatAge(172800, buf, sizeof(buf)); EXPECT_STREQ("2d", buf);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
