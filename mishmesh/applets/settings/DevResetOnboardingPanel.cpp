#include <mishmesh/applets/settings/DevResetOnboardingPanel.h>
#include <mishmesh/core/Canvas.h>

namespace mishmesh {

int DevResetOnboardingPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  (void)x; (void)y; (void)w; (void)h;
  _confirm.draw(c, 0, 0, c.width(), c.height());
  return 100;
}

bool DevResetOnboardingPanel::onInput(InputEvent ev) {
  if (_confirm.onInput(ev)) {
    ConfirmResult r = _confirm.result();
    if (r == ConfirmResult::Confirmed) {
      if (_app) _app->resetOnboarding();   // sets flag + reboots; does not return
      return true;
    }
    if (r == ConfirmResult::Cancelled) return false;   // bubble -> detail pops to list
    return true;   // NavLeft/NavRight toggled the selection
  }
  return false;
}

DevResetOnboardingPanel& devResetOnboardingSettings() {
  static DevResetOnboardingPanel s;
  return s;
}

}  // namespace mishmesh
