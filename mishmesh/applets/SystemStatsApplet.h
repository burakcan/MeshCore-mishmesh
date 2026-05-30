#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

static const int SYSSTATS_LINE_LEN  = 40;
static const int SYSSTATS_MAX_LINES = 8;

// Pure formatter: writes one display line per known metric into `out`, returning
// the number of lines written (<= maxLines). Unknown metrics render as "--";
// RAM total and firmware lines are omitted entirely when unknown.
int formatSystemStats(const SystemStats& s, char out[][SYSSTATS_LINE_LEN], int maxLines);

// Read-only "System" screen: a scrollable panel of device-health lines, rebuilt
// ~1s so free-heap stays live. NavUp/Down scroll; Back bubbles (host pops).
class SystemStatsApplet : public Applet {
  AppServices* _app;
  ScrollText   _panel;
  uint32_t     _lastBuilt;
  bool         _built;
  void rebuild(uint32_t now, bool keepScroll);
public:
  SystemStatsApplet()
      : Applet("System"), _app(nullptr), _lastBuilt(0), _built(false) {}
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
};

}  // namespace mishmesh
