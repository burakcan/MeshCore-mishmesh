#pragma once
#include <stdint.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

class Canvas;

// Generic vertical form. Rows = N text fields + a "Submit" row. Selecting a
// field pushes the shared KeypadApplet to edit its (caller-owned) buffer;
// selecting Submit validates each field then calls onSubmit. No heap: Field
// structs are copied into a fixed array, buffers stay owned by the caller.
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

  FormApplet() : Applet("Form") { _model.f = this; }

  void configure(const char* title, const Field* fields, int n,
                 FormSubmitFn onSubmit, void* ctx);

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int focusedRowForTest() const { return _list.selected(); }

private:
  bool submit();                 // true => caller should pop
  static void noopConfirm(void*, const char*) {}   // suppress keypad's own toast

  AppletHost*  _host = nullptr;
  const char*  _title = "";
  Field        _fields[MAX_FIELDS];
  int          _n = 0;
  FormSubmitFn _onSubmit = nullptr;
  void*        _ctx = nullptr;

  struct FormModel : ListModel {
    FormApplet* f = nullptr;
    int count() const override { return f->_n + 1; }   // fields + Submit
    const char* label(int i) const override {
      return i < f->_n ? f->_fields[i].label : "Submit";
    }
    const char* value(int i) const override {
      return i < f->_n ? f->_fields[i].buf : nullptr;
    }
  } _model;
  ListMenu _list;
};

FormApplet& formApplet();

}  // namespace mishmesh
