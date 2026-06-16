#pragma once

#include <mishmesh/core/Applet.h>       // AppletContext, AppServices
#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;

// A self-contained settings UI for one domain (Contacts/Messages/Advert).
// Concrete panels are singletons (e.g. contactsSettings()) so the in-app
// Settings tab and the Settings app's detail screen share one instance - data,
// logic, and widget state live in exactly one place.
//
// renderBody draws ONLY the body (the list, plus any modal overlay it owns) into
// the given rect; the host draws the surrounding header/tab chrome. A modal may
// draw full-screen via the Canvas's own width/height regardless of that rect.
// onInput returns true when it consumed the event; an unconsumed event (e.g.
// Back at the top level) bubbles to the host to pop or switch tabs.
class SettingsPanel {
public:
  virtual ~SettingsPanel() {}

  virtual const char* title() const = 0;
  virtual void begin(AppletContext& ctx) = 0;    // bind services; idempotent
  virtual void onShow() {}                         // refresh before display
  virtual int  renderBody(Canvas& c, int x, int y, int w, int h) = 0;
  virtual bool onInput(InputEvent ev) = 0;

  // True while a modal (stepper/confirm) is open: the host must route all input
  // to the panel and suppress tab switching until it closes.
  virtual bool modalActive() const { return false; }
};

}  // namespace mishmesh
