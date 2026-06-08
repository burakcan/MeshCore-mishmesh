#include <gtest/gtest.h>
#include <mishmesh/core/AppletStorage.h>
#include <map>
#include <string>
#include <vector>

using namespace mishmesh;

namespace {
// Minimal in-memory AppletStorage for tests.
class FakeStorage : public AppletStorage {
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

TEST(AppletStorage, SaveThenLoadRoundTrips) {
  FakeStorage s;
  uint8_t in[3] = {7, 8, 9};
  EXPECT_TRUE(s.save("k", in, 3));
  uint8_t out[8] = {0};
  EXPECT_EQ(3, s.load("k", out, 8));
  EXPECT_EQ(7, out[0]); EXPECT_EQ(8, out[1]); EXPECT_EQ(9, out[2]);
}

TEST(AppletStorage, LoadAbsentKeyReturnsZero) {
  FakeStorage s;
  uint8_t out[8] = {0};
  EXPECT_EQ(0, s.load("missing", out, 8));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
