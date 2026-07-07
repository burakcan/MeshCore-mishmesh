#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

class Canvas;

// Set a contact's routing path, rendered as a list: a Hash-size stepper row, a Path
// keypad row, and a Save button. Edits the caller's buffers in place; on Save calls
// the caller's submit (service call + toast), which returns true to pop this screen.
class SetPathApplet : public Applet, public ListModel {
public:
  SetPathApplet() : Applet("Set path") {}
  void configure(const char* title, char* pathBuf, uint16_t pathCap,
                 int* hashSize, int hsMin, int hsMax, StepperFormatFn hsLabel,
                 void (*pathFmt)(const char*, char*, uint16_t),
                 bool (*submit)(void*), void* ctx) {
    _title = title ? title : "Set path";
    _path = pathBuf; _pathCap = pathCap;
    _hash = hashSize; _hsMin = hsMin; _hsMax = hsMax; _hsLabel = hsLabel;
    _pathFmt = pathFmt; _submit = submit; _ctx = ctx;
    _editingHash = false;   // never carry a stale modal flag into a reused singleton
  }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const { return _editingHash; }

  // ListModel
  int count() const override { return 3; }
  const char* label(int i) const override;
  const char* value(int i) const override;
  bool isButton(int i) const override { return i == 2; }

private:
  AppServices* _app = nullptr;
  AppletHost*  _host = nullptr;
  const char* _title = "Set path";
  char*    _path = nullptr; uint16_t _pathCap = 0;
  int*     _hash = nullptr; int _hsMin = 1, _hsMax = 3;
  StepperFormatFn _hsLabel = nullptr;
  void  (*_pathFmt)(const char*, char*, uint16_t) = nullptr;
  bool  (*_submit)(void*) = nullptr;
  void*    _ctx = nullptr;
  ListMenu      _list;
  StatusBar     _bar;
  StepperDialog _stepper;
  bool          _editingHash = false;
  mutable char  _val[24];
};

SetPathApplet& setPathApplet();

}  // namespace mishmesh
