// Host (non-Arduino) implementation of the game's Platform seam, for native tests.
// Mirrors upstream's Desktop/ port: a RAM framebuffer, a canned button state, a
// deterministic RNG, and a RAM EEPROM. Offsets are relative (no base), matching how
// game.cpp calls Platform::(To|From)EEPROM.
#include "platform.h"
#include <cstring>

static uint8_t s_eeprom[128];
static uint8_t s_buttons = 0;
static uint8_t s_buffer[1024];

// Test hooks (declared extern in the test).
void HostSetButtons(uint8_t b) { s_buttons = b; }
void HostClearEeprom() { std::memset(s_eeprom, 0, sizeof(s_eeprom)); }

uint8_t Platform::ButtonState() { return s_buttons; }
uint8_t* Platform::GetBuffer() { return s_buffer; }
int16_t Platform::Random(int16_t i0, int16_t) { return i0; }   // deterministic
uint32_t Platform::GenerateRandomSeed() { return 0; }
void Platform::InitRandomSeed() {}
uint32_t Platform::Millis() { return 0; }

void Platform::DrawBuffer() {}
void Platform::PutPixel(uint8_t, uint8_t, uint8_t) {}
void Platform::DrawBitmap(const uint8_t*, int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
void Platform::DrawLine(int16_t, int16_t, int16_t, int16_t, uint8_t) {}
void Platform::DrawRect(int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
void Platform::DrawFilledRect(int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
void Platform::DrawCircle(int16_t, int16_t, uint8_t, uint8_t) {}
void Platform::DrawFilledCircle(int16_t, int16_t, uint8_t, uint8_t) {}
void Platform::FillScreen(uint8_t) {}
void Platform::Clear() {}

SavedState Platform::ToEEPROM(const uint8_t* bytes, const uint16_t offset, const uint16_t sz) {
  if (offset + sz > (uint16_t)sizeof(s_eeprom)) return TooBig;
  std::memcpy(s_eeprom + offset, bytes, sz);
  return Saved;
}
SavedState Platform::FromEEPROM(uint8_t* bytes, const uint16_t offset, const uint16_t sz) {
  if (offset + sz > (uint16_t)sizeof(s_eeprom)) return TooBig;
  std::memcpy(bytes, s_eeprom + offset, sz);
  return Saved;
}

#ifdef DEV_DEB
void Platform::DebugPrint(uint16_t, bool) {}
void Platform::DebugPrint(int16_t, bool) {}
void Platform::DebugPrint(float, bool) {}
void Platform::DebugPrint(const uint8_t*, bool) {}
#endif
