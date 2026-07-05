#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Button.h>

namespace mishmesh {

class Canvas;

// Pure boot-gate decision. state: 0=NOT_STARTED, 1=IN_PROGRESS, 2=DONE.
bool shouldShowOnboarding(uint8_t state, bool freshIdentity);

// Called once at Finish so the adapter can persist + hand off to the home screen.
typedef void (*OnboardDoneFn)(void* ctx);

// First-boot setup wizard. Stepped (Welcome, Device name, Radio region, GPS*, Time,
// All set; *GPS hidden when unsupported), each step a native screen: a title bar
// (step name + n/N) over a body. Welcome/Done draw the logo/summary + a Button. The
// middle steps are a ListMenu (label+value / toggle / radio rows). Name/GPS/Time end
// in a [Next] button row (rendered via the shared Button widget); Radio region is a
// preset list with no Next button - selecting a preset picks it and auto-advances.
// Input: NavUp/NavDown move focus, Select activates, Back = previous step. No left/right.
class OnboardingApplet : public Applet, public ListModel {
public:
  OnboardingApplet() : Applet("Setup") {}
  void configure(OnboardDoneFn done, void* ctx) { _done = done; _doneCtx = ctx; }

  void onStart(AppletContext& ctx) override;
  void onForeground() override {}
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel - rows for the current middle step (empty for Welcome/Done).
  int count() const override;
  const char* label(int i) const override;
  const char* value(int i) const override;
  bool isToggle(int i) const override;
  bool toggleState(int i) const override;
  bool isRadio(int i) const override;
  bool radioOn(int i) const override;
  bool isButton(int i) const override;

  // test seams
  int stepForTest() const { return _idx; }
  int stepCountForTest() const { return _stepCount; }
  int regionForTest() const { return _region; }
  int focusForTest() const { return _list.selected(); }
  int buttonRowForTest() const { return hasNextButton(cur()) ? count() - 1 : -1; }

private:
  enum Step : uint8_t { Welcome, Name, Region, Gps, Time, Done, STEP_MAX };
  static bool hasNextButton(Step s) { return s == Name || s == Gps || s == Time; }

  void buildSteps(bool gps);
  Step cur() const { return _steps[_idx]; }
  const char* stepTitle() const;
  void enterStep();
  void advance();
  void back();
  void activate(int row);
  void finish();
  void drawTitleBar(Canvas& c, int w);
  void drawCenterButton(Canvas& c, int x, int y, int w, const char* text);
  void drawWelcome(Canvas& c, int x, int y, int w, int h);
  void drawDone(Canvas& c, int x, int y, int w, int h);
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
  int  _region = 0;
  bool _gps = false;
  int  _tzCity = -1;
  bool _autoSync = true;

  ListMenu     _list;    // middle-step rows
  Button       _btn;     // Welcome/Done button
  mutable char _val[24]; // scratch (unused-safe)
};

}  // namespace mishmesh
