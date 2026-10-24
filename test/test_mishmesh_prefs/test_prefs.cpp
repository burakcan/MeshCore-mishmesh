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

// A key the parser rejects aborts the whole load, so every field after it silently
// keeps its default - and loadPrefs() ignores the false return. Before meshcore v1.18
// digits were rejected, which is how "t12h" was caught and why that key is "t_ampm".
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

// prefs.json exactly as mishmesh v1.6.1 on meshcore v1.17.1 wrote it: fewer radio
// keys than v1.18, no tz_offset, no "custom".
static const char V161_PREFS[] =
  "{name:\"Wio L1\",lat:52.370216,lon:4.895168,"
  "radio:{freq:869.6180,bw:62.5000,sf:8,cr:8,rxgain:1,tx:22,af:1.0000,rxdelay:0,hash_mode:1,multi_ack:1},"
  "gps:{en:1,int:300,adv_loc:1},repeat:{disable:1},"
  "comp:{auto_max:3,defs_nm:\"nl\",defs_key:\"A0A1A2A3A4A5A6A7A8A9AAABACADAEAF\",pin:123456,"
  "buzz_q:1,vibe_q:0,auto_add:30,man_add:1,tel_base:1,tel_loc:1,tel_env:2},"
  "mm:{v:1,vol:3,mute:10,tone_ch:5,tone_dm:7,tz_q:8,tz_city:42,t_ampm:1,t_manual:1,datefmt:2,"
  "sleep:4,bright:3,slpscr:2,slprot:1,wakehome:3,rpt_freq:867.5000,ble_en:0,cfull_notif:0,onboard:2}}";

// The same node as stock meshcore v1.18.0 writes it.
static const char STOCK_V118_PREFS[] =
  "{name:\"Wio L1\",lat:52.370216,lon:4.895168,"
  "radio:{freq:869.6180,bw:62.5000,sf:8,cr:8,cad:1,int_thr:0,rxgain:1,fem_rxgain:0,fem_txgain:0,tx:22,"
  "af:1.0000,rxdelay:0,f_txdelay:0.5000,d_txdelay:0.2000,agc_int:0,hash_mode:1,multi_ack:1},"
  "gps:{en:1,int:300,adv_loc:1},repeat:{disable:1},"
  "comp:{auto_max:3,defs_nm:\"nl\",defs_key:\"A0A1A2A3A4A5A6A7A8A9AAABACADAEAF\",pin:123456,"
  "buzz_q:1,vibe_q:0,auto_add:30,man_add:1,tel_base:1,tel_loc:1,tel_env:2,tz_offset:2},"
  "custom:{}}";

TEST(NodePrefsSerial, LoadsFileWrittenByV161) {
  NodePrefs p;
  MemStream r; r.buf = V161_PREFS;
  ASSERT_TRUE(p.loadSerial(r));

  EXPECT_NE(p.mm_ver, 0);
  EXPECT_EQ(p.sound_volume, 3);
  EXPECT_EQ(p.sound_mute_mask, 0x0A);
  EXPECT_EQ(p.notify_tone_ch, 5);
  EXPECT_EQ(p.notify_tone_dm, 7);
  EXPECT_EQ(p.tz_quarter_hours, 8);
  EXPECT_EQ(p.tz_city_index, 42);
  EXPECT_EQ(p.time_fmt_12h, 1);
  EXPECT_EQ(p.manual_time_set, 1);
  EXPECT_EQ(p.date_format, 2);
  EXPECT_EQ(p.screen_sleep, 4);
  EXPECT_EQ(p.screen_brightness, 3);
  EXPECT_EQ(p.sleep_screen, 2);
  EXPECT_EQ(p.sleep_rotation, 1);
  EXPECT_EQ(p.wake_home, 3);
  EXPECT_FLOAT_EQ(p.repeat_saved_freq, 867.5f);
  EXPECT_EQ(p.ble_enabled, 0);
  EXPECT_EQ(p.contacts_full_notify, 0);
  EXPECT_EQ(p.onboarding_state, 2);

  EXPECT_STREQ(p.node_name, "Wio L1");
  EXPECT_FLOAT_EQ(p.freq, 869.618f);
  EXPECT_EQ(p.tx_power_dbm, 22);
  EXPECT_EQ(p.ble_pin, 123456u);
  EXPECT_EQ(p.default_scope_key[15], 0xAF);
  EXPECT_EQ(p.telemetry_mode_env, 2);
  // keys v1.18 added are absent from the old file
  EXPECT_EQ(p.tz_offset, 0);
  EXPECT_EQ(p.cad_enabled, 0);
}

TEST(NodePrefsSerial, StockV118FileLeavesMishmeshAtDefaults) {
  NodePrefs p;
  MemStream r; r.buf = STOCK_V118_PREFS;
  ASSERT_TRUE(p.loadSerial(r));

  // DataStore reads mm_ver == 0 as "first mishmesh boot" and starts onboarding.
  EXPECT_EQ(p.mm_ver, 0);
  NodePrefs d;
  EXPECT_EQ(p.sound_volume, d.sound_volume);
  EXPECT_EQ(p.sound_mute_mask, d.sound_mute_mask);
  EXPECT_EQ(p.notify_tone_ch, d.notify_tone_ch);
  EXPECT_EQ(p.notify_tone_dm, d.notify_tone_dm);
  EXPECT_EQ(p.tz_quarter_hours, d.tz_quarter_hours);
  EXPECT_EQ(p.tz_city_index, d.tz_city_index);
  EXPECT_EQ(p.time_fmt_12h, d.time_fmt_12h);
  EXPECT_EQ(p.manual_time_set, d.manual_time_set);
  EXPECT_EQ(p.date_format, d.date_format);
  EXPECT_EQ(p.screen_sleep, d.screen_sleep);
  EXPECT_EQ(p.screen_brightness, d.screen_brightness);
  EXPECT_EQ(p.sleep_screen, d.sleep_screen);
  EXPECT_EQ(p.sleep_rotation, d.sleep_rotation);
  EXPECT_EQ(p.wake_home, d.wake_home);
  EXPECT_FLOAT_EQ(p.repeat_saved_freq, d.repeat_saved_freq);
  EXPECT_EQ(p.ble_enabled, d.ble_enabled);
  EXPECT_EQ(p.contacts_full_notify, d.contacts_full_notify);
  EXPECT_EQ(p.onboarding_state, d.onboarding_state);

  EXPECT_STREQ(p.node_name, "Wio L1");
  EXPECT_FLOAT_EQ(p.freq, 869.618f);
  EXPECT_EQ(p.cad_enabled, 1);
  EXPECT_FLOAT_EQ(p.tx_delay_factor, 0.5f);
  EXPECT_FLOAT_EQ(p.direct_tx_delay_factor, 0.2f);
  EXPECT_EQ(p.tz_offset, 2);
  EXPECT_EQ(p.telemetry_mode_env, 2);
}

// The v1.17 parser fails on an empty object, so a v1.6.1 build reading this file
// after a downgrade stops at "custom:{}". Everything written before it still loads,
// which is why "mm" has to come first.
TEST(NodePrefsSerial, MishmeshBlockIsWrittenBeforeCustom) {
  NodePrefs a;
  fillNonDefault(a);
  a.mm_ver = 1;

  MemStream empty;
  ASSERT_TRUE(a.saveSerial(empty));
  size_t mm = empty.buf.find("mm:{");
  size_t custom = empty.buf.find("custom:{}");
  ASSERT_NE(mm, std::string::npos) << empty.buf;
  ASSERT_NE(custom, std::string::npos) << empty.buf;
  EXPECT_LT(mm, custom);

  ASSERT_TRUE(a.getCustom()->setByKey("note", "abc"));
  MemStream w;
  ASSERT_TRUE(a.saveSerial(w));
  custom = w.buf.find("custom:{note:\"abc\"}");
  ASSERT_NE(custom, std::string::npos) << w.buf;
  EXPECT_LT(w.buf.find("mm:{"), custom);

  NodePrefs b;
  MemStream r; r.buf = w.buf;
  ASSERT_TRUE(b.loadSerial(r));
  EXPECT_EQ(b.mm_ver, 1);
  EXPECT_EQ(b.onboarding_state, 2);
  char val[16] = "";
  // upstream getByKey terminates at value[max_len], so leave it a byte
  EXPECT_TRUE(b.getCustom()->getByKey("note", val, sizeof(val) - 1));
  EXPECT_STREQ(val, "abc");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
