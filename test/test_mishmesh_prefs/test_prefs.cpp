#include <gtest/gtest.h>
#include <string>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../examples/companion_radio/NodePrefs.h"

namespace {

// std::string-backed Stream. The mock Print only stubs print(), so the numeric
// overloads have to be real here or nothing lands in the buffer.
class MemStream : public Stream {
public:
  std::string buf;
  size_t rd = 0;

  size_t write(uint8_t b) override { buf.push_back((char)b); return 1; }
  size_t write(const uint8_t* p, size_t n) override { buf.append((const char*)p, n); return n; }
  int available() override { return (int)(buf.size() - rd); }
  int read() override { return rd < buf.size() ? (uint8_t)buf[rd++] : -1; }
  int peek() override { return rd < buf.size() ? (uint8_t)buf[rd] : -1; }

  size_t print(unsigned char v, int = 10) override { return fmt("%u", (unsigned)v); }
  size_t print(int v, int = 10) override { return fmt("%d", v); }
  size_t print(unsigned int v, int = 10) override { return fmt("%u", v); }
  size_t print(long v, int = 10) override { return fmt("%ld", v); }
  size_t print(unsigned long v, int = 10) override { return fmt("%lu", v); }
  size_t print(long long v, int = 10) override { return fmt("%lld", v); }
  size_t print(unsigned long long v, int = 10) override { return fmt("%llu", v); }
  size_t print(double v, int p = 2) override { return fmt("%.*f", p, v); }

private:
  size_t fmt(const char* f, ...) {
    char t[48];
    va_list ap; va_start(ap, f);
    int n = vsnprintf(t, sizeof(t), f, ap);
    va_end(ap);
    if (n < 0) return 0;
    buf.append(t);
    return (size_t)n;
  }
};

void fillNonDefault(NodePrefs& p) {
  strcpy(p.node_name, "TestNode");
  p.freq = 869.525f; p.bw = 250.0f; p.sf = 11; p.cr = 5;
  p.sound_volume = 3;
  p.sound_mute_mask = 0x0A;
  p.notify_tone_ch = 5;
  p.notify_tone_dm = 7;
  p.tz_quarter_hours = 8;
  p.tz_city_index = 42;
  p.time_fmt_12h = 1;
  p.manual_time_set = 1;
  p.date_format = 2;
  p.screen_sleep = 4;
  p.screen_brightness = 3;
  p.repeat_saved_freq = 867.5f;
  p.ble_enabled = 0;
  p.contacts_full_notify = 0;
  p.onboarding_state = 2;
}

} // namespace

// ConfigSerializer's is_key_char() accepts [A-Za-z_] only. A digit in a key aborts
// the whole parse, so every field defined after it in structure() silently keeps its
// default - and loadPrefs() ignores the false return. This caught "t12h".
TEST(NodePrefsSerial, AllKeysAreParseable) {
  NodePrefs a;
  fillNonDefault(a);

  MemStream w;
  ASSERT_TRUE(a.saveSerial(w));

  NodePrefs b;
  MemStream r; r.buf = w.buf;
  EXPECT_TRUE(b.loadSerial(r)) << "parse aborted; JSON was: " << w.buf;
}

TEST(NodePrefsSerial, MishmeshBlockRoundTrips) {
  NodePrefs a;
  fillNonDefault(a);

  MemStream w;
  ASSERT_TRUE(a.saveSerial(w));

  NodePrefs b;
  MemStream r; r.buf = w.buf;
  b.loadSerial(r);

  EXPECT_EQ(b.sound_volume, 3);
  EXPECT_EQ(b.sound_mute_mask, 0x0A);
  EXPECT_EQ(b.notify_tone_ch, 5);
  EXPECT_EQ(b.notify_tone_dm, 7);
  EXPECT_EQ(b.tz_quarter_hours, 8);
  EXPECT_EQ(b.tz_city_index, 42);
  EXPECT_EQ(b.time_fmt_12h, 1);
  EXPECT_EQ(b.manual_time_set, 1);
  EXPECT_EQ(b.date_format, 2);
  EXPECT_EQ(b.screen_sleep, 4);
  EXPECT_EQ(b.screen_brightness, 3);
  EXPECT_FLOAT_EQ(b.repeat_saved_freq, 867.5f);
  EXPECT_EQ(b.ble_enabled, 0);
  EXPECT_EQ(b.contacts_full_notify, 0);
  EXPECT_EQ(b.onboarding_state, 2);
}

TEST(NodePrefsSerial, CoreFieldsRoundTrip) {
  NodePrefs a;
  fillNonDefault(a);

  MemStream w;
  ASSERT_TRUE(a.saveSerial(w));

  NodePrefs b;
  MemStream r; r.buf = w.buf;
  b.loadSerial(r);

  EXPECT_STREQ(b.node_name, "TestNode");
  EXPECT_FLOAT_EQ(b.freq, 869.525f);
  EXPECT_FLOAT_EQ(b.bw, 250.0f);
  EXPECT_EQ(b.sf, 11);
  EXPECT_EQ(b.cr, 5);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
