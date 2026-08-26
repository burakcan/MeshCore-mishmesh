#pragma once

#include <stdint.h>

namespace mishmesh {

struct AppletStorage;
struct AppletRegistration;

// Cross-cutting UI preferences: battery indicator style and the Home screen's
// left/right quick-action shortcuts. Values are cached in RAM and persisted
// via AppletStorage ("uibatt", "battcal", "qa_l", "qa_r"). Quick actions are stored as
// the applet's registered label string so no registry changes are needed; an
// unresolvable label falls back to the slot default (Contacts / Messages).
// begin(nullptr) is valid (host tests, headless): defaults apply, sets no-op.
class UiPrefs {
public:
  enum class BattMode : uint8_t { Gauge = 0, Percent = 1, Voltage = 2 };

  static const int SLOT_LEFT = 0;
  static const int SLOT_RIGHT = 1;
  static const int LABEL_CAP = 17;   // stored label + NUL (registry labels are short)

  void begin(AppletStorage* s);

  BattMode battMode() const { return _battMode; }
  void     setBattMode(BattMode m);           // persists "uibatt"

  int  battCalPercent() const { return _battCal; }   // 50..150, 100 = no trim
  void setBattCalPercent(int pct);            // clamps, persists "battcal"

  // The raw configured label (stored value, else the slot default).
  const char* quickActionLabel(int slot) const;
  void setQuickAction(int slot, const char* label);
  // Resolve against Placement::AppMenu registrations: configured label first,
  // slot default second, nullptr when neither exists in this build.
  const AppletRegistration* quickAction(int slot) const;

  // Display theme: dark (light-on-black, the default) or inverted "light"
  // mode. Canvas resolves every color through this ("uithm").
  bool darkMode() const { return _dark; }
  void setDarkMode(bool on);

  void resetForTest();

private:
  AppletStorage* _st = nullptr;
  BattMode _battMode = BattMode::Gauge;
  int      _battCal = 100;
  bool _dark = true;
  char _qa[2][LABEL_CAP] = {{0}, {0}};
};

UiPrefs& uiPrefs();

}  // namespace mishmesh
