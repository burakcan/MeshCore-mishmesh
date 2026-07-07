// mishmesh/applets/AboutApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/QrView.h>

namespace mishmesh {

// "About mishmesh": app-menu credits + support screen. Shows the mishmesh
// wordmark, firmware version, and a Ko-fi donation QR (drawn theme-neutral so it
// scans in either mode). Read-only; Back pops. App-menu singleton.
class AboutApplet : public Applet {
public:
  AboutApplet() : Applet("About") {}
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Test hook.
  const char* qrTextForTest() const;

private:
  AppServices* _app = nullptr;
  char   _version[24] = {0};
  QrView _qr;
};

AboutApplet& aboutApplet();

}  // namespace mishmesh
