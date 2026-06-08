#include <gtest/gtest.h>
#include <mishmesh/arduboy/Eeprom.h>
#include <mishmesh/core/AppletStorage.h>
#include <map>
#include <string>
#include <vector>

using namespace mishmesh;
using namespace mishmesh::arduboy;

namespace {
class FakeStorage : public AppletStorage {
public:
  std::map<std::string, std::vector<uint8_t>> data;
  uint8_t load(const char* k, uint8_t* dst, uint8_t cap) override {
    auto it = data.find(k); if (it == data.end()) return 0;
    uint8_t n = (uint8_t)std::min<size_t>(it->second.size(), cap);
    for (uint8_t i = 0; i < n; i++) dst[i] = it->second[i]; return n;
  }
  bool save(const char* k, const uint8_t* s, uint8_t len) override {
    data[k] = std::vector<uint8_t>(s, s + len); return true;
  }
};
}

TEST(EepromImage, PutGetRoundTrips) {
  EepromImage e;
  uint16_t v = 1234;
  e.put(16, v);
  uint16_t out = 0; e.get(16, out);
  EXPECT_EQ(1234, out);
}

TEST(EepromImage, WriteMarksDirtyAndSavePersistsThenLoadRestores) {
  FakeStorage s;
  EepromImage e;
  EXPECT_FALSE(e.dirty());
  e.write(20, 0xAB);
  EXPECT_TRUE(e.dirty());
  e.saveIfDirty(&s, "hopper");
  EXPECT_FALSE(e.dirty());           // cleared after save

  EepromImage e2;
  e2.load(&s, "hopper");
  EXPECT_EQ(0xAB, e2.read(20));
}

TEST(EepromImage, UpdateDoesNotDirtyWhenUnchanged) {
  EepromImage e;
  e.write(5, 0x10);
  e.saveIfDirtyMarkClean();          // helper: clear dirty without storage
  e.update(5, 0x10);                 // same value
  EXPECT_FALSE(e.dirty());
  e.update(5, 0x11);                 // changed
  EXPECT_TRUE(e.dirty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
