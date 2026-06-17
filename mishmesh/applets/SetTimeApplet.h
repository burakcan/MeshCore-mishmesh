#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/TimeFormat.h>

namespace mishmesh {

// Manual date/time editor. Numeric fields shown in the locale's date order with
// separators (e.g. DD/MM/YYYY  HH:MM). NavLeft/Right move between fields in that
// display order; NavUp/Down change the selected field (wrap + day clamp); Select
// commits (local -> UTC via the current tz offset) and pops; Back cancels.
class SetTimeApplet : public Applet {
public:
  SetTimeApplet() : Applet("Set Time") {}

  void configure(AppServices* app);   // seed from current local time; call before push

  void onStart(AppletContext& ctx) override { _host = ctx.host; }
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // test-only
  const LocalTime& editing() const { return _lt; }
  int field() const { return _field; }

private:
  enum Field : int { Day, Month, Year, Hour, Minute, FIELD_COUNT };
  void step(int delta);
  void buildOrder();          // fill _order from _dateFmt (date part) + time
  int  activeField() const { return _order[_field]; }   // logical field at cursor

  AppServices* _app = nullptr;
  AppletHost*  _host = nullptr;
  LocalTime    _lt{};
  int16_t      _offset = 0;
  DateFormat   _dateFmt = DateFormat::DMY;
  int          _order[FIELD_COUNT] = { Day, Month, Year, Hour, Minute };
  int          _field = 0;    // cursor position in display order
};

SetTimeApplet& setTimeApplet();   // shared singleton

}  // namespace mishmesh
