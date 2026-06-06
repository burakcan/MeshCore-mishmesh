// mishmesh/applets/MessagePathApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

class MessagePathApplet : public Applet {
public:
  MessagePathApplet() : Applet("Path") {}
  void setTarget(const ConvoKey& k, int msgIdx) { _key = k; _idx = msgIdx; }
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int  rowCountForTest() const { return _rows; }
  const char* titleForTest() const { return _title; }
  const char* lineForTest(int i) const { return _text.lineForTest(i); }
private:
  void rebuild();
  void hopLabel(uint8_t b, char* out, size_t cap) const;   // name if known, else %02X
  MessagesService* _svc = nullptr;
  AppServices*     _app = nullptr;
  ConvoKey         _key{};
  int              _idx = 0;
  ScrollText       _text;
  StatusBar        _bar;
  char             _title[20] = {0};   // StatusBar holds a pointer; this backs it
  int              _rows = 0;
};

MessagePathApplet& messagePathApplet();

}  // namespace mishmesh
