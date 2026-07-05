#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

class Canvas;

// Pure boot-gate decision. state: 0=NOT_STARTED, 1=IN_PROGRESS, 2=DONE.
// DONE never shows; IN_PROGRESS always resumes; NOT_STARTED shows only on a
// freshly-generated identity (a brand-new or factory-reset device).
bool shouldShowOnboarding(uint8_t state, bool freshIdentity);

// Called once at Finish so the adapter can persist + hand off to the home screen.
typedef void (*OnboardDoneFn)(void* ctx);

// First-boot setup wizard: Welcome, Device name, Region, GPS (only if supported),
// Time, All set. Root applet during onboarding; pushes KeypadApplet (name),
// timezonePickerApplet (zone) and setTimeApplet (manual clock) as children. Values
// are staged and applied at Finish, then the done callback hands off.
class OnboardingApplet : public Applet {
public:
  OnboardingApplet() : Applet("Setup") {}
  void configure(OnboardDoneFn done, void* ctx) { _done = done; _doneCtx = ctx; }

  void onStart(AppletContext& ctx) override;
  void onForeground() override;   // no-op: children stage via their callbacks
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // test seams
  int  stepForTest() const { return _idx; }
  int  stepCountForTest() const { return _stepCount; }
  const char* nameForTest() const { return _name; }
  int  regionForTest() const { return _regionList.selected(); }

private:
  enum Step : uint8_t { Welcome, Name, Region, Gps, Time, Done, STEP_MAX };

  void buildSteps(bool gps);
  Step cur() const { return _steps[_idx]; }
  void next();
  void back();
  void finish();
  static void onNameDone(void* ctx, const char* text);
  static void onTzPicked(void* ctx, int cityIndex);

  AppServices*  _app = nullptr;
  AppletHost*   _host = nullptr;
  OnboardDoneFn _done = nullptr;
  void*         _doneCtx = nullptr;

  Step _steps[STEP_MAX];
  int  _stepCount = 0;
  int  _idx = 0;

  // staged state (applied at Finish)
  char _name[32] = {0};
  bool _gps = false;
  int  _tzCity = -1;
  bool _autoSync = true;
  int  _timeRow = 0;     // 0 = zone, 1 = auto-sync, 2 = set clock (manual only)

  ListMenu _regionList;  // region = _regionList.selected() (PRESETS index)
};

}  // namespace mishmesh
