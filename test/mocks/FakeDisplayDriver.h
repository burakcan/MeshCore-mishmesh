#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <string>
#include <vector>

// Records draw calls so tests can assert on clipped/translated geometry.
class FakeDisplayDriver : public DisplayDriver {
public:
  struct Rect { int x, y, w, h; };

  bool on = true;
  std::vector<std::string> calls;     // ordered log of high-level calls
  std::vector<Rect> fills;            // fillRect args (device coords)
  std::vector<Rect> rects;            // drawRect args (device coords)
  Color lastColor = DARK;
  int cursorX = 0, cursorY = 0;
  std::vector<std::string> prints;

  FakeDisplayDriver(int w = 128, int h = 64) : DisplayDriver(w, h) {}

  bool isOn() override { return on; }
  void turnOn() override { on = true; }
  void turnOff() override { on = false; }
  void clear() override { calls.push_back("clear"); }
  void startFrame(Color bkg = DARK) override { (void)bkg; calls.push_back("startFrame"); }
  void endFrame() override { calls.push_back("endFrame"); }
  void setTextSize(int) override {}
  void setColor(Color c) override { lastColor = c; }
  void setCursor(int x, int y) override { cursorX = x; cursorY = y; }
  void print(const char* str) override { prints.push_back(str ? str : ""); }
  void fillRect(int x, int y, int w, int h) override { fills.push_back({x, y, w, h}); }
  void drawRect(int x, int y, int w, int h) override { rects.push_back({x, y, w, h}); }
  void drawXbm(int, int, const uint8_t*, int, int) override {}
  uint16_t getTextWidth(const char* str) override { return str ? (uint16_t)(6 * __builtin_strlen(str)) : 0; }
};
