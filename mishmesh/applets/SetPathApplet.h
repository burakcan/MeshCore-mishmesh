#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>
#include <mishmesh/widgets/Card.h>

namespace mishmesh {

// Manual routing path editor for a contact, reached from the contact detail menu.
// Three rows: a hash-size carousel (1/2/3 bytes per hop), a hex Path field edited
// via the shared keypad ("aa,bb,cc"), and Save. Launch-only, like ContactDetailApplet.
class SetPathApplet : public Applet, public ListModel {
public:
  enum Row { HashSize, Path, Save, ROW_COUNT };

  // Parse comma-separated hex hops ("aa,bb" / "aabb,ccdd") into raw bytes, with
  // `hashSize` bytes per hop. Returns the hop count, or -1 on malformed input.
  // `out` must hold PATH_MAX_BYTES. Empty/whitespace text => 0 hops. (static for tests)
  static int parseHexPath(const char* text, uint8_t hashSize, uint8_t* out, int outCap);

private:
  AppletHost*      _host;
  ContactsService* _svc;
  uint8_t          _pubkey[6];
  char             _name[44];        // display name (may carry a repeater key prefix)
  char             _info[48];        // "Type | distance | last heard" — same as the detail card
  bool             _favourite;
  uint8_t          _hashSize;        // 1..3 bytes per hop
  char             _pathBuf[160];    // hex text, edited by the keypad
  ListMenu         _list;
  StepperDialog    _sizeStepper;
  Card             _card;            // header: same name+info card as the contact detail page
  bool             _editingSize;

public:
  SetPathApplet();
  // name/info/favourite mirror the contact detail card so the header is identical.
  void setTarget(const uint8_t* pubKey, const char* name, const char* info = "", bool favourite = false);
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  // ListModel
  int count() const override { return ROW_COUNT; }
  const char* label(int i) const override;
  const char* value(int i) const override;

  // Test seam: set the path text and current hash size, then drive Select on Save.
  void setPathTextForTest(const char* s, uint8_t hashSize);
};

SetPathApplet& setPathApplet();

}  // namespace mishmesh
