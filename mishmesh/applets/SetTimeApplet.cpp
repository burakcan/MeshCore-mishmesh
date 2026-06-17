#include <mishmesh/applets/SetTimeApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void SetTimeApplet::buildOrder() {
  int d0 = Day, d1 = Month, d2 = Year;
  if (_dateFmt == DateFormat::MDY) { d0 = Month; d1 = Day; d2 = Year; }
  else                             { d0 = Day; d1 = Month; d2 = Year; }   // DMY, DMonY
  _order[0] = d0; _order[1] = d1; _order[2] = d2;
  _order[3] = Hour; _order[4] = Minute;
}

void SetTimeApplet::configure(AppServices* app) {
  _app = app;
  _offset = app ? app->tzOffsetMinutes() : 0;
  _dateFmt = (DateFormat)(app ? app->dateFormat() : 0);
  buildOrder();
  uint32_t e = app ? app->epochSeconds() : 0;
  if (e == 0) e = 1767225600u;   // 2026-01-01 00:00 UTC fallback when clock unset
  _lt = applyTz(e, _offset);
  _field = 0;
}

static void clampDay(LocalTime& lt) {
  uint8_t dim = daysInMonth(lt.year, lt.month);
  if (lt.day > dim) lt.day = dim;
  if (lt.day < 1) lt.day = 1;
}

void SetTimeApplet::step(int delta) {
  switch (activeField()) {
    case Day: {
      uint8_t dim = daysInMonth(_lt.year, _lt.month);
      int d = _lt.day + delta;
      while (d < 1) d += dim;
      while (d > dim) d -= dim;
      _lt.day = (uint8_t)d;
      break;
    }
    case Month: {
      int m = _lt.month + delta;
      while (m < 1) m += 12;
      while (m > 12) m -= 12;
      _lt.month = (uint8_t)m;
      clampDay(_lt);
      break;
    }
    case Year:
      _lt.year = (int16_t)(_lt.year + delta);
      if (_lt.year < 2020) _lt.year = 2020;
      if (_lt.year > 2099) _lt.year = 2099;
      clampDay(_lt);
      break;
    case Hour: {
      int hh = (_lt.hour + delta) % 24;
      if (hh < 0) hh += 24;
      _lt.hour = (uint8_t)hh;
      break;
    }
    case Minute: {
      int mm = (_lt.minute + delta) % 60;
      if (mm < 0) mm += 60;
      _lt.minute = (uint8_t)mm;
      break;
    }
  }
}

bool SetTimeApplet::onInput(InputEvent ev) {
  switch (ev) {
    case InputEvent::NavLeft:  _field = (_field + FIELD_COUNT - 1) % FIELD_COUNT; return true;
    case InputEvent::NavRight: _field = (_field + 1) % FIELD_COUNT; return true;
    case InputEvent::NavUp:    step(+1); return true;
    case InputEvent::NavDown:  step(-1); return true;
    case InputEvent::Select:
      _lt.second = 0;
      if (_app) _app->setEpochSeconds(composeUtc(_lt, _offset));
      if (_host) _host->pop();
      return true;
    default:
      return false;   // Back/Cancel bubble -> host pops
  }
}

int SetTimeApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int fh = c.fontHeight(fontBody());
  c.drawText(fontBody(), w / 2, 2, "Set date & time",
             DisplayDriver::LIGHT, TextAlign::Center);

  // Field text, indexed by logical Field, rendered to match the chosen format.
  bool nameMonth = (_dateFmt == DateFormat::DMonY);
  char s[FIELD_COUNT][6];
  snprintf(s[Day], sizeof(s[Day]), nameMonth ? "%u" : "%02u", _lt.day);
  if (nameMonth)
    snprintf(s[Month], sizeof(s[Month]), "%s",
             (_lt.month >= 1 && _lt.month <= 12) ? MONTHS[_lt.month - 1] : "");
  else
    snprintf(s[Month], sizeof(s[Month]), "%02u", _lt.month);
  snprintf(s[Year],   sizeof(s[Year]),   "%u",   _lt.year);
  snprintf(s[Hour],   sizeof(s[Hour]),   "%02u", _lt.hour);
  snprintf(s[Minute], sizeof(s[Minute]), "%02u", _lt.minute);

  auto drawField = [&](int x, int y, const char* t, bool sel) -> int {
    int tw = c.textWidth(fontBody(), t);
    if (sel) c.fillRect(x - 1, y - 1, tw + 2, fh + 2, DisplayDriver::LIGHT);
    c.drawText(fontBody(), x, y, t, sel ? DisplayDriver::DARK : DisplayDriver::LIGHT,
               TextAlign::Left);
    return tw;
  };
  auto sepW = [&](const char* t) { return c.textWidth(fontBody(), t); };

  const char* dsep = nameMonth ? " " : "/";
  int dateY = h / 2 - fh - 1;
  int timeY = h / 2 + 3;

  // Date line: three fields in display order, packed with separators, centered.
  int dw = 0;
  for (int i = 0; i < 3; i++) { dw += sepW(s[_order[i]]); if (i < 2) dw += sepW(dsep); }
  int x = (w - dw) / 2;
  for (int i = 0; i < 3; i++) {
    x += drawField(x, dateY, s[_order[i]], _field == i);
    if (i < 2) { c.drawText(fontBody(), x, dateY, dsep, DisplayDriver::LIGHT, TextAlign::Left); x += sepW(dsep); }
  }

  // Time line: HH:MM, packed, centered.
  int tw = sepW(s[_order[3]]) + sepW(":") + sepW(s[_order[4]]);
  x = (w - tw) / 2;
  x += drawField(x, timeY, s[_order[3]], _field == 3);
  c.drawText(fontBody(), x, timeY, ":", DisplayDriver::LIGHT, TextAlign::Left); x += sepW(":");
  x += drawField(x, timeY, s[_order[4]], _field == 4);

  c.drawText(fontBody(), w / 2, h - fh, "Select to save",
             DisplayDriver::LIGHT, TextAlign::Center);
  return 500;
}

SetTimeApplet& setTimeApplet() {
  static SetTimeApplet s;
  return s;
}

}  // namespace mishmesh
