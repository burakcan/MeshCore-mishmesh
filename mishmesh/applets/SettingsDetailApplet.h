#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Full-screen host for one SettingsPanel, pushed by the Settings app. Draws a
// title/battery header and hands the body to the panel; Back bubbles to pop.
class SettingsDetailApplet : public Applet {
public:
  SettingsDetailApplet() : Applet("Settings") {}
  void setPanel(SettingsPanel* p) { _panel = p; }   // call before push()

  void onStart(AppletContext& ctx) override;
  void onStop() override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

private:
  SettingsPanel* _panel = nullptr;
  AppServices*   _app   = nullptr;
  StatusBar      _bar;
};

SettingsDetailApplet& settingsDetailApplet();   // shared singleton

}  // namespace mishmesh
