#include <mishmesh/applets/SystemStatsApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const uint32_t REBUILD_MS = 1000;

// "142.3K", or "--" when zero (unknown).
static void fmtBytes(char* buf, int n, uint32_t bytes) {
  if (bytes == 0) { snprintf(buf, n, "--"); return; }
  snprintf(buf, n, "%u.%uK", (unsigned)(bytes / 1024),
           (unsigned)((bytes % 1024) * 10 / 1024));
}

int formatSystemStats(const SystemStats& s, char out[][SYSSTATS_LINE_LEN], int maxLines) {
  int n = 0;
  char v[12];
#define SS_EMIT(...) do { if (n < maxLines) snprintf(out[n++], SYSSTATS_LINE_LEN, __VA_ARGS__); } while (0)
  fmtBytes(v, sizeof(v), s.heapFreeBytes);    SS_EMIT("Heap free: %s", v);
  fmtBytes(v, sizeof(v), s.heapMinFreeBytes); SS_EMIT("Heap min: %s", v);
  if (s.heapTotalBytes) { fmtBytes(v, sizeof(v), s.heapTotalBytes); SS_EMIT("Heap total: %s", v); }
  SS_EMIT("Contacts: %u/%u", (unsigned)s.contactsUsed, (unsigned)s.contactsMax);
  if (s.storageTotalKb)
    SS_EMIT("Storage: %u/%uK", (unsigned)s.storageUsedKb, (unsigned)s.storageTotalKb);
  else
    SS_EMIT("Storage: --");
  if (s.batteryMv == 0)
    SS_EMIT("Battery: --");
  else
    SS_EMIT("Battery: %u.%02uV", (unsigned)(s.batteryMv / 1000),
            (unsigned)((s.batteryMv % 1000) / 10));
  uint32_t mins = s.uptimeSecs / 60;
  SS_EMIT("Uptime: %uh %02um", (unsigned)(mins / 60), (unsigned)(mins % 60));
  if (s.firmwareVersion) SS_EMIT("FW: %s", s.firmwareVersion);
#undef SS_EMIT
  return n;
}

void SystemStatsApplet::rebuild(uint32_t now, bool keepScroll) {
  _lastBuilt = now;
  _built = true;
  _panel.clear(keepScroll);   // periodic refresh keeps the user's scroll position
  SystemStats s;
  if (!(_app && _app->systemStats(s))) {
    _panel.addLine("Stats unavailable");
    return;
  }
  char lines[SYSSTATS_MAX_LINES][SYSSTATS_LINE_LEN];
  int n = formatSystemStats(s, lines, SYSSTATS_MAX_LINES);
  for (int i = 0; i < n; i++) _panel.addLine(lines[i]);
}

void SystemStatsApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _built = false;
  _panel.clear();
}

int SystemStatsApplet::onRender(Canvas& c) {
  uint32_t now = c.now();
  if (!_built) rebuild(now, false);
  else if (now - _lastBuilt >= REBUILD_MS) rebuild(now, true);
  _panel.draw(c, 0, 0, c.width(), c.height());
  return _panel.needsAnimation() ? 33 : (int)REBUILD_MS;
}

bool SystemStatsApplet::onInput(InputEvent ev) {
  return _panel.onInput(ev);   // NavUp/Down scroll; everything else bubbles
}

static SystemStatsApplet s_systemStats;
MISHMESH_REGISTER_APPLET_ICON(&s_systemStats, ::mishmesh::Placement::AppMenu,
                              "System", 0, (uint16_t)::mishmesh::Icon::Chip);

}  // namespace mishmesh
