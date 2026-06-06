#pragma once
#include <stdint.h>
#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Generic vertical form rendered as labelled input boxes plus a button. NavUp/
// NavDown move focus across the fields and the button; Select on a field opens
// the shared KeypadApplet to edit its (caller-owned) buffer; Select on the
// button validates each field then calls onSubmit. No heap: Field structs are
// copied into a fixed array, buffers stay owned by the caller.
class FormApplet : public Applet {
public:
  static const int MAX_FIELDS = 3;

  struct Field {
    const char* label;
    char*       buf;        // caller-owned, NUL-terminated
    uint16_t    cap;        // buffer capacity incl. NUL
    bool      (*validate)(const char*);  // null => non-empty check
    const char* errMsg;     // toast posted when validate fails
  };
  typedef bool (*FormSubmitFn)(void* ctx);  // return true => form pops

  FormApplet() : Applet("Form") {}

  void configure(const char* title, const Field* fields, int n,
                 FormSubmitFn onSubmit, void* ctx,
                 const char* submitLabel = "Submit");

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int focusedRowForTest() const { return _focus; }   // 0.._n-1 = fields, _n = button

private:
  bool submit();                 // true => caller should pop
  static void noopConfirm(void*, const char*) {}   // suppress keypad's own toast

  AppletHost*  _host = nullptr;
  const char*  _title = "";
  const char*  _submitLabel = "Submit";
  Field        _fields[MAX_FIELDS];
  int          _n = 0;
  int          _focus = 0;       // 0.._n-1 = fields, _n = button row
  int          _scroll = 0;      // vertical scroll offset (px), for tall forms
  FormSubmitFn _onSubmit = nullptr;
  void*        _ctx = nullptr;
};

FormApplet& formApplet();

}  // namespace mishmesh
