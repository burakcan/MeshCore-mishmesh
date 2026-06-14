#pragma once
#include <stdint.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/StatusBar.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

class Canvas;

// buf -> display label for a Text field's value box (e.g. raw hex -> "2 hops").
typedef void (*TextFormatFn)(const char* buf, char* out, uint16_t cap);

// Generic vertical form rendered as labelled input boxes plus a button under a
// StatusBar top bar. NavUp/NavDown move focus across the fields and the button.
// A Text field's Select opens the shared KeypadApplet to edit its (caller-owned)
// buffer; a Stepper field's Select opens a StepperDialog modal that writes back
// its caller-owned int. Select on the button validates each field then calls
// onSubmit. No heap: Field structs are copied into a fixed array; buffers/values
// stay owned by the caller.
class FormApplet : public Applet {
public:
  static const int MAX_FIELDS = 3;

  enum Kind : uint8_t { Text = 0, Stepper = 1 };

  struct Field {
    const char* label;
    char*       buf;        // Text: caller-owned, NUL-terminated
    uint16_t    cap;        // Text: buffer capacity incl. NUL
    bool      (*validate)(const char*);  // null => non-empty check (Text only)
    const char* errMsg;     // toast posted when validate fails
    // appended; omitted in a brace-init => null/0/Text (existing call sites unaffected):
    TextFormatFn    display;  // Text: buf -> box label (null => show raw buf)
    uint8_t         kind;     // 0 = Text, 1 = Stepper
    int*            ival;     // Stepper: caller-owned value (read + written)
    int             imin, imax;
    StepperFormatFn ifmt;     // Stepper: value -> label
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
  AppServices* _app = nullptr;
  StatusBar    _bar;
  StepperDialog _stepper;
  bool         _editingStepper = false;
  int          _stepperField = -1;
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
