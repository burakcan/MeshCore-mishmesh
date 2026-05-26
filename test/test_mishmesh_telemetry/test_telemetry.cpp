#include <gtest/gtest.h>
#include <mishmesh/core/TelemetryDecode.h>
#include <helpers/sensors/LPPDataHelpers.h>
#include <cstring>
using namespace mishmesh;

// Build LPP: channel, type, big-endian payload.
static uint8_t* put(uint8_t* p, uint8_t ch, uint8_t type) { *p++ = ch; *p++ = type; return p; }

TEST(Telemetry, DecodesVoltageAndTemperature) {
  uint8_t buf[16]; uint8_t* p = buf;
  p = put(p, 1, LPP_VOLTAGE);     uint16_t v = (uint16_t)(3.92f * 100); *p++ = v >> 8; *p++ = v & 0xFF;
  p = put(p, 3, LPP_TEMPERATURE); int16_t t = (int16_t)(21.4f * 10);    *p++ = t >> 8; *p++ = t & 0xFF;
  TelemetryView view{};
  decodeTelemetry(buf, (uint8_t)(p - buf), view);
  ASSERT_TRUE(view.valid);
  ASSERT_EQ(2, view.count);
  EXPECT_EQ(LPP_VOLTAGE, view.fields[0].type);
  EXPECT_NEAR(3.92f, view.fields[0].value, 0.01f);
  EXPECT_EQ(LPP_TEMPERATURE, view.fields[1].type);
  EXPECT_NEAR(21.4f, view.fields[1].value, 0.05f);
}

TEST(Telemetry, SkipsUnknownType) {
  uint8_t buf[8]; uint8_t* p = buf;
  p = put(p, 5, LPP_LUMINOSITY); *p++ = 0x01; *p++ = 0x00;   // unknown-to-reader, 2 bytes
  p = put(p, 1, LPP_VOLTAGE);    *p++ = 0x01; *p++ = 0x90;
  TelemetryView view{};
  decodeTelemetry(buf, (uint8_t)(p - buf), view);
  EXPECT_TRUE(view.valid);
  // Voltage must still be found after skipping the luminosity field.
  bool foundV = false;
  for (int i = 0; i < view.count; i++) if (view.fields[i].type == LPP_VOLTAGE) foundV = true;
  EXPECT_TRUE(foundV);
}

TEST(Telemetry, FormatsWithUnits) {
  TelemetryField f{1, LPP_VOLTAGE, 3.92f, 0, 0};
  char b[32]; formatField(f, b, sizeof(b));
  EXPECT_NE(nullptr, strstr(b, "V"));
  EXPECT_STREQ("Voltage", telemTypeName(LPP_VOLTAGE));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
