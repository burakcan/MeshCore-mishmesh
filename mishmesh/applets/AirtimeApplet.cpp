#include <mishmesh/applets/AirtimeApplet.h>
#include <mishmesh/core/AirtimeHistory.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const int BAR_H = 13;

// Compact airtime duration: "12s" under a minute, "5m" under an hour, else
// "1h2m". Airtime totals span seconds (fresh boot) to hours (long-running RX).
static void fmtDur(char* out, size_t cap, uint32_t ms) {
  uint32_t s = ms / 1000u;
  if (s < 60u)        snprintf(out, cap, "%us", (unsigned)s);
  else if (s < 3600u) snprintf(out, cap, "%um", (unsigned)(s / 60u));
  else                snprintf(out, cap, "%uh%um",
                               (unsigned)(s / 3600u), (unsigned)((s / 60u) % 60u));
}

AirtimeApplet::AirtimeApplet() : Applet("Airtime") {}

void AirtimeApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _tabs.clear();
  _tabs.addTab("Budget", (uint16_t)Icon::Hourglass);   // duty-cycle time budget
  _tabs.addTab("History", (uint16_t)Icon::Radio);      // RF activity over time
  _tab = 0;
  _tabs.setSelected(0);
}

int AirtimeApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  _tabs.setBattery(_app ? _app->batteryMillivolts() : 0);
  _tabs.draw(c, 0, 0, w, BAR_H);
  int bodyY = BAR_H + 1;
  int bodyH = h - bodyY;

  if (!_app || !_app->airtimeStats(_st)) {
    c.drawText(fontBody(), w / 2, bodyY + bodyH / 2 - c.fontHeight(fontBody()) / 2,
               "No airtime data", DisplayDriver::LIGHT, TextAlign::Center);
    return 1000;
  }
  return _tab == TAB_HISTORY ? renderHistory(c, bodyY, bodyH)
                             : renderBudget(c, bodyY, bodyH);
}

int AirtimeApplet::renderBudget(Canvas& c, int y, int h) {
  int w = c.width();
  const int LW = 62;   // left (gauge) column width

  // --- left: big "free %" hero + labelled fill bar ---
  unsigned pct = _st.txBudgetMax ? (unsigned)((uint64_t)_st.txBudgetMs * 100u / _st.txBudgetMax) : 0;
  if (pct > 100) pct = 100;
  char num[8];
  snprintf(num, sizeof(num), "%u", pct);
  int nh = c.fontHeight(fontNum());
  c.drawText(fontNum(), 3, y + 1, num, DisplayDriver::LIGHT);
  int nw = c.textWidth(fontNum(), num);
  c.drawText(fontBody(), 3 + nw + 1, y + 1 + nh - c.fontHeight(fontBody()), "%",
             DisplayDriver::LIGHT);
  c.drawText(fontBody(), 3, y + 2 + nh, "TX free", DisplayDriver::LIGHT);

  int barW = LW - 6;
  int barY = y + h - 7;
  c.drawRect(3, barY, barW, 6, DisplayDriver::LIGHT);
  int fillW = _st.txBudgetMax ? (barW - 2) * (int)pct / 100 : 0;
  if (fillW > 0) c.fillRect(4, barY + 1, fillW, 4, DisplayDriver::LIGHT);

  // --- right: lifetime + last-hour stats, label left / value right ---
  int rowH = c.lineHeight(fontBody());
  int x0 = LW + 2;
  int ly = y + 1;
  char v[14];
  fmtDur(v, sizeof(v), _st.txTotalMs);
  c.drawText(fontBody(), x0, ly, "TX", DisplayDriver::LIGHT);
  c.drawText(fontBody(), w, ly, v, DisplayDriver::LIGHT, TextAlign::Right);
  ly += rowH;
  fmtDur(v, sizeof(v), _st.rxTotalMs);
  c.drawText(fontBody(), x0, ly, "RX", DisplayDriver::LIGHT);
  c.drawText(fontBody(), w, ly, v, DisplayDriver::LIGHT, TextAlign::Right);
  ly += rowH;
  fmtDur(v, sizeof(v), _st.history ? _st.history->txWindowMs() : 0);
  c.drawText(fontBody(), x0, ly, "1h", DisplayDriver::LIGHT);
  c.drawText(fontBody(), w, ly, v, DisplayDriver::LIGHT, TextAlign::Right);
  ly += rowH;
  snprintf(v, sizeof(v), "%u/%u", (unsigned)(_st.sentFlood + _st.sentDirect),
           (unsigned)(_st.recvFlood + _st.recvDirect));
  c.drawText(fontBody(), x0, ly, "Pkt", DisplayDriver::LIGHT);
  c.drawText(fontBody(), w, ly, v, DisplayDriver::LIGHT, TextAlign::Right);

  return 1000;   // budget refills continuously; keep the % live
}

int AirtimeApplet::renderHistory(Canvas& c, int y, int h) {
  int w = c.width();
  const AirtimeHistory* hist = _st.history;
  if (!hist) {
    c.drawText(fontBody(), w / 2, y + h / 2 - c.fontHeight(fontBody()) / 2,
               "No history", DisplayDriver::LIGHT, TextAlign::Center);
    return 1000;
  }

  int capH = c.lineHeight(fontCaption());
  int n = hist->bucketCount();
  int gx = 3;
  int barW = (w - gx - 3) / n;
  if (barW < 1) barW = 1;
  int drawW = barW > 1 ? barW - 1 : 1;
  int spanW = barW * n;
  int baseline = y + h - capH - 2;
  int graphTop = y + 1;
  int graphH = baseline - graphTop;
  if (graphH < 4) graphH = 4;

  // Scale so both the tallest stacked bucket and the per-minute budget line fit.
  uint32_t scale = hist->maxBucket(true);
  uint32_t perMin = _st.txBudgetMax / (uint32_t)n;
  if (perMin > scale) scale = perMin;
  if (scale < 1) scale = 1;

  for (int i = 0; i < n; i++) {
    uint32_t tx = hist->txBucket(i), rx = hist->rxBucket(i);
    int hTx = (int)((uint64_t)tx * graphH / scale);
    int hRx = (int)((uint64_t)rx * graphH / scale);
    if (hTx > graphH) hTx = graphH;
    if (hTx + hRx > graphH) hRx = graphH - hTx;
    int x = gx + i * barW;
    if (hTx > 0) c.fillRect(x, baseline - hTx, drawW, hTx, DisplayDriver::LIGHT);
    if (hRx > 0) c.fillStipple(x, baseline - hTx - hRx, drawW, hRx, DisplayDriver::LIGHT);
  }

  // Dashed per-minute TX-budget line (the sustainable ceiling for one bucket).
  if (_st.txBudgetMax > 0) {
    int ty = baseline - (int)((uint64_t)perMin * graphH / scale);
    if (ty < graphTop) ty = graphTop;
    c.fillStipple(gx, ty, spanW, 1, DisplayDriver::LIGHT);
  }
  // Baseline axis.
  c.fillRect(gx, baseline + 1, spanW, 1, DisplayDriver::LIGHT);

  // Bottom caption: window span + TX/RX airtime over the hour.
  char tx[12], rx[12], line[28];
  fmtDur(tx, sizeof(tx), hist->txWindowMs());
  fmtDur(rx, sizeof(rx), hist->rxWindowMs());
  snprintf(line, sizeof(line), "TX %s  RX %s", tx, rx);
  c.drawText(fontCaption(), gx, y + h - capH, "1h", DisplayDriver::LIGHT);
  c.drawText(fontCaption(), w, y + h - capH, line, DisplayDriver::LIGHT, TextAlign::Right);
  return 1000;
}

bool AirtimeApplet::onInput(InputEvent ev) {
  if (_tabs.onInput(ev)) { _tab = _tabs.selected(); return true; }
  return false;   // Back bubbles -> host pops
}

AirtimeApplet& airtimeApplet() {
  static AirtimeApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&airtimeApplet(), Placement::AppMenu, "Airtime", 6,
                              (uint16_t)Icon::Reload);   // cycle arrows = duty cycle

}  // namespace mishmesh
