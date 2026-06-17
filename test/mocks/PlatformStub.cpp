// Weak Platform:: stubs so the vendored game2048 object files link cleanly in every
// native test binary.  The real (strong) implementations live in
// test/test_mishmesh_game2048/host_platform.cpp and override these for that test.
#include "platform.h"
#include <cstring>

__attribute__((weak)) uint8_t Platform::ButtonState() { return 0; }
__attribute__((weak)) uint8_t* Platform::GetBuffer() { return nullptr; }
__attribute__((weak)) int16_t Platform::Random(int16_t i0, int16_t) { return i0; }
__attribute__((weak)) uint32_t Platform::GenerateRandomSeed() { return 0; }
__attribute__((weak)) void Platform::InitRandomSeed() {}
__attribute__((weak)) uint32_t Platform::Millis() { return 0; }

__attribute__((weak)) void Platform::DrawBuffer() {}
__attribute__((weak)) void Platform::PutPixel(uint8_t, uint8_t, uint8_t) {}
__attribute__((weak)) void Platform::DrawBitmap(const uint8_t*, int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
__attribute__((weak)) void Platform::DrawLine(int16_t, int16_t, int16_t, int16_t, uint8_t) {}
__attribute__((weak)) void Platform::DrawRect(int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
__attribute__((weak)) void Platform::DrawFilledRect(int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
__attribute__((weak)) void Platform::DrawCircle(int16_t, int16_t, uint8_t, uint8_t) {}
__attribute__((weak)) void Platform::DrawFilledCircle(int16_t, int16_t, uint8_t, uint8_t) {}
__attribute__((weak)) void Platform::FillScreen(uint8_t) {}
__attribute__((weak)) void Platform::Clear() {}

__attribute__((weak)) SavedState Platform::ToEEPROM(const uint8_t*, const uint16_t, const uint16_t) { return NotSaved; }
__attribute__((weak)) SavedState Platform::FromEEPROM(uint8_t*, const uint16_t, const uint16_t) { return NotSaved; }

#ifdef DEV_DEB
__attribute__((weak)) void Platform::DebugPrint(uint16_t, bool) {}
__attribute__((weak)) void Platform::DebugPrint(int16_t, bool) {}
__attribute__((weak)) void Platform::DebugPrint(float, bool) {}
__attribute__((weak)) void Platform::DebugPrint(const uint8_t*, bool) {}
#endif
