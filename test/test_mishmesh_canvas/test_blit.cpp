#include <gtest/gtest.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

// A 1bpp column-major buffer: byte = 8 vertical px, LSB = top. Build an 8x8 buffer with
// only the top-left pixel (col 0, row 0 -> byte 0, bit 0) and the pixel at col1,row1.
TEST(Blit1bpp, PortableDefaultSetsExpectedPixels) {
  FakeDisplayDriver d(8, 8);            // 8 columns x 8 rows -> 8 bytes
  uint8_t buf[8] = {0};
  buf[0] = 0x01;                         // col 0: bit0 set -> pixel (0,0)
  buf[1] = 0x02;                         // col 1: bit1 set -> pixel (1,1)
  Canvas c(&d);
  c.blit1bpp(buf, 8, 8);
  // The default emits a 1x1 fill per lit pixel. Expect exactly two lit pixels.
  ASSERT_EQ(2u, d.litPixels.size());
  EXPECT_TRUE(d.hasLit(0, 0));
  EXPECT_TRUE(d.hasLit(1, 1));
  EXPECT_FALSE(d.hasLit(0, 1));
}
