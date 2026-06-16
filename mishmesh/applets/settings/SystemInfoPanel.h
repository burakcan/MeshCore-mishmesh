#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

static const int SYSSTATS_LINE_LEN  = 40;
static const int SYSSTATS_MAX_LINES = 8;

// Pure formatter: one display line per known metric into `out`, returning the
// count (<= maxLines). Unknown metrics render as "--"; RAM total and firmware
// lines are omitted entirely when unknown.
int formatSystemStats(const SystemStats& s, char out[][SYSSTATS_LINE_LEN], int maxLines);

// Read-only device-health panel: a scrollable list rebuilt ~1s so free-heap
// stays live. NavUp/Down scroll; Back bubbles.
class SystemInfoPanel : public SettingsPanel {
public:
  const char* title() const override { return "System Info"; }
  void begin(AppletContext& ctx) override;
  void onShow() override { _built = false; }   // rebuild on (re)entry
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override { return _panel.onInput(ev); }

  // test seam: read a built line (e.g. assert "Stats unavailable")
  const char* lineForTest(int i) const { return _panel.lineForTest(i); }

private:
  void rebuild(uint32_t now, bool keepScroll);
  AppServices* _app = nullptr;
  ScrollText   _panel;
  uint32_t     _lastBuilt = 0;
  bool         _built = false;
};

SystemInfoPanel& systemInfoSettings();   // shared singleton

}  // namespace mishmesh
