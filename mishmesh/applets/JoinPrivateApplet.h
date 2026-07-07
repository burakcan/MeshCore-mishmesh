#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class Canvas;

// Join a private channel by name + 32-hex key, rendered as a list (Name / Key rows +
// a Join button). Edits the caller's buffers in place via the keypad; validates the
// key with the caller's validator; on Join calls the caller's submit (which does the
// service call + toast and returns true to pop this screen).
class JoinPrivateApplet : public Applet, public ListModel {
public:
  JoinPrivateApplet() : Applet("Join private") {}
  void configure(char* nameBuf, uint16_t nameCap, char* keyBuf, uint16_t keyCap,
                 bool (*keyValid)(const char*), bool (*submit)(void*), void* ctx) {
    _name = nameBuf; _nameCap = nameCap; _key = keyBuf; _keyCap = keyCap;
    _keyValid = keyValid; _submit = submit; _ctx = ctx;
  }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel
  int count() const override { return 3; }
  const char* label(int i) const override;
  const char* value(int i) const override;
  bool isButton(int i) const override { return i == 2; }

  int selectedForTest() const { return _list.selected(); }

private:
  AppServices* _app = nullptr;
  AppletHost*  _host = nullptr;
  char*    _name = nullptr; uint16_t _nameCap = 0;
  char*    _key = nullptr;  uint16_t _keyCap = 0;
  bool  (*_keyValid)(const char*) = nullptr;
  bool  (*_submit)(void*) = nullptr;
  void*    _ctx = nullptr;
  ListMenu  _list;
  StatusBar _bar;
};

JoinPrivateApplet& joinPrivateApplet();

}  // namespace mishmesh
