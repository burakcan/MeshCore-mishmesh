#include <mishmesh/applets/NavigationApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/text/Fonts.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const char* NAV_KEY = "navwp";
static const uint16_t NAV_MAGIC = 0x4E57;  // "WN"
static const uint8_t NAV_VERSION = 1;

void NavigationApplet::onStart(AppletContext& ctx) {
  _ctx = &ctx;
  _nav = -1;
  _sel = 0;
  _dirty = false;
  load();
}

void NavigationApplet::load() {
  _count = 0;
  if (!_ctx || !_ctx->storage) return;

  uint8_t buf[128]{};
  uint8_t n = _ctx->storage->load(NAV_KEY, buf, sizeof(buf));
  if (n < 4) return;
  uint16_t magic = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
  if (magic != NAV_MAGIC || buf[2] != NAV_VERSION) return;
  uint8_t count = buf[3];
  if (count > MAX_WAYPOINTS) count = MAX_WAYPOINTS;
  const uint8_t rec = sizeof(Waypoint);
  if ((uint16_t)4 + (uint16_t)count * rec > n) return;
  memcpy(_wp, buf + 4, (size_t)count * rec);
  for (uint8_t i = 0; i < count; i++) _wp[i].label[LABEL_LEN - 1] = '\0';
  _count = count;
}

void NavigationApplet::save() {
  if (!_ctx || !_ctx->storage) return;
  uint8_t buf[128]{};
  buf[0] = (uint8_t)(NAV_MAGIC & 0xFF);
  buf[1] = (uint8_t)(NAV_MAGIC >> 8);
  buf[2] = NAV_VERSION;
  buf[3] = _count;
  memcpy(buf + 4, _wp, (size_t)_count * sizeof(Waypoint));
  _ctx->storage->save(NAV_KEY, buf, (uint8_t)(4 + _count * sizeof(Waypoint)));
  _dirty = false;
}

bool NavigationApplet::ownLocation(int32_t& lat, int32_t& lon) const {
  if (!_ctx || !_ctx->contacts || !_ctx->app) return false;
  if (!_ctx->app->gpsEnabled() || !_ctx->app->gpsHasFix()) return false;
  return _ctx->contacts->selfLocation(lat, lon);
}

void NavigationApplet::markHere() {
  if (_count >= MAX_WAYPOINTS) return;
  int32_t lat, lon;
  if (!ownLocation(lat, lon)) return;

  Waypoint& w = _wp[_count];
  w.lat = lat;
  w.lon = lon;
  snprintf(w.label, sizeof(w.label), "WP%u", (unsigned)(_count + 1));
  _count++;
  _dirty = true;
  save();
  if (_sel >= _count) _sel = _count - 1;
}

void NavigationApplet::renameWaypoint() {
  if (!_ctx || !_ctx->host || _sel < 0 || _sel >= _count) return;
  keypadApplet().configure(_wp[_sel].label, LABEL_LEN - 1, "Waypoint name",
                           &NavigationApplet::onRenameDone, this);
  _ctx->host->push(&keypadApplet());
}

void NavigationApplet::onRenameDone(void* ctx, const char* text) {
  NavigationApplet* self = static_cast<NavigationApplet*>(ctx);
  if (!self || !text) return;
  if (self->_sel < 0 || self->_sel >= self->_count) return;
  if (text[0] == '\0') return;
  strncpy(self->_wp[self->_sel].label, text, LABEL_LEN - 1);
  self->_wp[self->_sel].label[LABEL_LEN - 1] = '\0';
  self->save();
  if (self->_ctx && self->_ctx->host) self->_ctx->host->postToast("Name saved");
}

void NavigationApplet::deleteWaypoint() {
  if (_sel < 0 || _sel >= _count) return;
  for (int i = _sel; i + 1 < _count; i++) _wp[i] = _wp[i + 1];
  _count--;
  if (_sel >= _count && _count > 0) _sel = _count - 1;
  if (_count == 0) _sel = 0;
  save();
}

float NavigationApplet::haversineKm(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
  const float R = 6371.0f;
  const float K = 0.01745329251994329577f * 1e-6f;
  float a1 = lat1 * K, a2 = lat2 * K;
  float da = (lat2 - lat1) * K, db = (lon2 - lon1) * K;
  float s1 = sinf(da * 0.5f), s2 = sinf(db * 0.5f);
  float a = s1*s1 + cosf(a1)*cosf(a2)*s2*s2;
  if (a > 1.0f) a = 1.0f;
  return R * 2.0f * asinf(sqrtf(a));
}

int NavigationApplet::bearingDeg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
  const float K = 0.01745329251994329577f * 1e-6f;
  float a1 = lat1 * K, a2 = lat2 * K;
  float dl = (lon2 - lon1) * K;
  float b = atan2f(sinf(dl)*cosf(a2), cosf(a1)*sinf(a2) - sinf(a1)*cosf(a2)*cosf(dl));
  int d = (int)(b * 57.29577951308232f + 0.5f);
  if (d < 0) d += 360;
  return d % 360;
}

const char* NavigationApplet::cardinal(int deg) {
  static const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
  return dirs[((deg + 22) % 360) / 45];
}

void NavigationApplet::formatDistance(char* out, int cap, float km) {
  if (km < 1.0f) snprintf(out, cap, "%dm", (int)(km * 1000.0f + 0.5f));
  else if (km < 100.0f) snprintf(out, cap, "%.1fkm", km);
  else snprintf(out, cap, "%dkm", (int)(km + 0.5f));
}

void NavigationApplet::renderList(Canvas& c) {
  c.drawText(fontBody(), 2, 3, "NAVIGATION", DisplayDriver::LIGHT);
  char line[40];
  const int top = 25;
  const int step = 18;
  for (int i = 0; i < (int)_count + 1; i++) {
    bool selected = (i == _sel);
    int y = top + i * step;
    if (selected) c.fillRect(0, y - 2, c.width(), step, DisplayDriver::LIGHT);
    if (i == _count) {
      snprintf(line, sizeof(line), "+ Zapisz tutaj%s", _count >= MAX_WAYPOINTS ? " (full)" : "");
    } else {
      snprintf(line, sizeof(line), "%s", _wp[i].label);
    }
    c.drawText(fontBody(), 4, y, line, selected ? DisplayDriver::DARK : DisplayDriver::LIGHT);
  }

  // Keep the footer deliberately short: the L1 panel is only 250px wide.
  // Short labels also match the pixel-style UI better than a long sentence.
  c.drawText(fontCaption(), 3, c.height() - 11,
             "OK:OTWÓRZ  L:NAZWA  B:USUŃ", DisplayDriver::LIGHT);
}

static void navLine(Canvas& c, int x0, int y0, int x1, int y1, DisplayDriver::Color color) {
  // Small integer Bresenham line. Canvas intentionally has no general line
  // primitive, so navigation uses a handful of 1px pixels for the arrow.
  int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
  int sx = x0 < x1 ? 1 : -1;
  int dy = -(y1 >= y0 ? y1 - y0 : y0 - y1);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    c.fillRect(x0, y0, 1, 1, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void navArrow(Canvas& c, int cx, int cy, int deg, DisplayDriver::Color color) {
  // Pixel-art navigation arrow. 0 degrees = north, clockwise.
  const float rad = (float)deg * 0.01745329251994329577f;
  const float sx = sinf(rad);
  const float sy = -cosf(rad);
  const float px = -sy;
  const float py = sx;

  const int tip = 26;
  const int base = 7;
  const int wing = 9;
  const int tx = (int)lroundf(cx + sx * tip);
  const int ty = (int)lroundf(cy + sy * tip);
  const int bx = (int)lroundf(cx - sx * base);
  const int by = (int)lroundf(cy - sy * base);
  const int lx = (int)lroundf(tx - sx * 12 + px * wing);
  const int ly = (int)lroundf(ty - sy * 12 + py * wing);
  const int rx = (int)lroundf(tx - sx * 12 - px * wing);
  const int ry = (int)lroundf(ty - sy * 12 - py * wing);

  // Pixel-art shaft.
  navLine(c, bx, by, tx, ty, color);
  navLine(c, bx + (int)lroundf(px), by + (int)lroundf(py),
          tx + (int)lroundf(px), ty + (int)lroundf(py), color);
  navLine(c, bx - (int)lroundf(px), by - (int)lroundf(py),
          tx - (int)lroundf(px), ty - (int)lroundf(py), color);
  // Arrow head.
  navLine(c, lx, ly, tx, ty, color);
  navLine(c, rx, ry, tx, ty, color);
  // Small center hub.
  c.fillRect(cx - 1, cy - 1, 3, 3, color);
}

static void navCompass(Canvas& c, int cx, int cy, int r, int bearing) {
  // Pixel ring with four cardinal ticks and a few intermediate ticks.
  c.drawArc(cx, cy, r, 2, 0, 359, DisplayDriver::LIGHT);
  c.drawArc(cx, cy, r - 4, 1, 0, 359, DisplayDriver::LIGHT);

  const float pi = 3.14159265358979323846f;
  for (int d = 0; d < 360; d += 45) {
    float a = d * pi / 180.0f;
    int x1 = (int)lroundf(cx + sinf(a) * (r - 1));
    int y1 = (int)lroundf(cy - cosf(a) * (r - 1));
    int x2 = (int)lroundf(cx + sinf(a) * (r - 6));
    int y2 = (int)lroundf(cy - cosf(a) * (r - 6));
    navLine(c, x1, y1, x2, y2, DisplayDriver::LIGHT);
  }

  c.drawText(fontCaption(), cx, cy - r - 1, "N", DisplayDriver::LIGHT, TextAlign::Center);
  c.drawText(fontCaption(), cx + r + 1, cy - 4, "E", DisplayDriver::LIGHT, TextAlign::Left);
  c.drawText(fontCaption(), cx, cy + r - 5, "S", DisplayDriver::LIGHT, TextAlign::Center);
  c.drawText(fontCaption(), cx - r - 1, cy - 4, "W", DisplayDriver::LIGHT, TextAlign::Right);

  navArrow(c, cx, cy, bearing, DisplayDriver::LIGHT);
}

void NavigationApplet::renderNav(Canvas& c) {
  if (_nav < 0 || _nav >= _count) { _nav = -1; return; }
  int32_t lat, lon;

  // Compact pixel-style header.
  c.drawText(fontCaption(), 3, 2, "NAV", DisplayDriver::LIGHT);
  char header[24];
  snprintf(header, sizeof(header), "-> %s", _wp[_nav].label);
  c.drawText(fontCaption(), c.width() / 2, 2, header, DisplayDriver::LIGHT, TextAlign::Center);

  if (!ownLocation(lat, lon)) {
    c.drawTextCentered(fontBody(), 0, 38, c.width(), 28, "BRAK FIX GPS", DisplayDriver::LIGHT);
    c.drawText(fontCaption(), 3, c.height() - 11, "Wstecz = lista", DisplayDriver::LIGHT);
    return;
  }

  float km = haversineKm(lat, lon, _wp[_nav].lat, _wp[_nav].lon);
  int deg = bearingDeg(lat, lon, _wp[_nav].lat, _wp[_nav].lon);
  char dist[16], line[48];
  formatDistance(dist, sizeof(dist), km);

  // Large pixel-font distance.
  c.drawTextScaled(fontBody(), c.width() / 2, 13, dist,
                   DisplayDriver::LIGHT, 2, TextAlign::Center);

  // Compass/ring is deliberately compact for the 250x122 e-ink panel.
  const int cx = c.width() / 2;
  const int cy = 64;
  const int r = 31;
  navCompass(c, cx, cy, r, deg);

  snprintf(line, sizeof(line), "%03d %s", deg, cardinal(deg));
  c.drawText(fontBody(), cx, 96, line, DisplayDriver::LIGHT, TextAlign::Center);

  // Bottom pixel info row: waypoint name + coordinates, kept short enough for 250px.
  snprintf(line, sizeof(line), "%s  %.5f %.5f", _wp[_nav].label,
           _wp[_nav].lat / 1e6f, _wp[_nav].lon / 1e6f);
  c.drawText(fontCaption(), cx, 110, line, DisplayDriver::LIGHT, TextAlign::Center);
}

int NavigationApplet::onRender(Canvas& c) {
  c.fillRect(0, 0, c.width(), c.height(), DisplayDriver::DARK);
  if (_nav >= 0) {
    renderNav(c);
    // E-ink does not benefit from very frequent redraws. GPS data is updated
    // independently; 1.5 s keeps the navigation responsive without needless
    // panel refreshes.
    return 1500;
  }
  renderList(c);
  return 1000;
}

bool NavigationApplet::onInput(InputEvent ev) {
  if (_nav >= 0) {
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _nav = -1; return true; }
    return true;
  }

  const int total = (int)_count + 1;
  if (ev == InputEvent::Back || ev == InputEvent::Cancel) return false;
  if (ev == InputEvent::NavUp) {
    _sel = (_sel > 0) ? _sel - 1 : total - 1;
    return true;
  }
  if (ev == InputEvent::NavDown) {
    _sel = (_sel + 1 < total) ? _sel + 1 : 0;
    return true;
  }
  if (ev == InputEvent::SelectLong) {
    if (_sel < _count) renameWaypoint();
    return true;
  }
  if (ev == InputEvent::BackLong) {
    if (_sel < _count) deleteWaypoint();
    return true;
  }
  if (ev == InputEvent::Select) {
    if (_sel == _count) {
      markHere();
      return true;
    }
    if (_sel < _count) {
      _nav = _sel;
      return true;
    }
  }
  return true;
}

NavigationApplet& navigationApplet() {
  static NavigationApplet s_navigation;
  return s_navigation;
}

void registerNavigationApplet() {
  static AppletRegistration reg = {
      &navigationApplet(), "Nawigacja", (uint16_t)::mishmesh::Icon::Gps,
      ::mishmesh::Placement::AppMenu, 8, nullptr};
  static bool registered = false;
  if (!registered) {
    ::mishmesh::registerApplet(&reg);
    registered = true;
  }
}

}  // namespace mishmesh
