#pragma once
#include <mishmesh/core/Applet.h>

namespace mishmesh {

// Full-screen "contacts store full" banner, pushed by the adapter the first time
// the contact count reaches capacity while auto-add can no longer make room
// ("Overwrite oldest" off). Informational: one soft alert tone, any key
// dismisses, auto-closes after a few seconds. A reused singleton
// (contactsFullApplet()); the counts are copied in via raise() so the banner text
// is stable regardless of later store churn.
class ContactsFullApplet : public Applet {
public:
  ContactsFullApplet() : Applet("ContactsFull") {}

  void raise(uint16_t used, uint16_t max) {
    _used = used; _max = max; _raisedAt = 0; _beeped = false;
  }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  uint16_t usedForTest() const { return _used; }
  uint16_t maxForTest() const { return _max; }

private:
  void dismiss();

  AppletHost*         _host  = nullptr;
  sound::SoundEngine* _sound = nullptr;
  uint16_t _used = 0, _max = 0;
  uint32_t _raisedAt = 0;   // 0 => stamp on next render (auto-dismiss base)
  bool     _beeped = false;
};

ContactsFullApplet& contactsFullApplet();

}  // namespace mishmesh
