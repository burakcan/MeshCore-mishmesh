#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

// Dev-only settings panel: a single confirm that re-triggers onboarding and reboots.
// Confirm -> AppServices::resetOnboarding() (does not return); Cancel/Back bubbles so
// the settings detail pops back to the list.
class DevResetOnboardingPanel : public SettingsPanel {
public:
  const char* title() const override { return "Reset onboarding"; }
  void begin(AppletContext& ctx) override { _app = ctx.app; }
  void onShow() override { _confirm.configure("Reset onboarding & reboot?"); }
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

private:
  AppServices*  _app = nullptr;
  ConfirmDialog _confirm;
};

DevResetOnboardingPanel& devResetOnboardingSettings();

}  // namespace mishmesh
