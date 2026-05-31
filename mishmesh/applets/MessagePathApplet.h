// mishmesh/applets/MessagePathApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

class MessagePathApplet : public Applet {
public:
  MessagePathApplet() : Applet("Path") {}
  void setTarget(const ConvoKey& k, int msgIdx) { _key = k; _idx = msgIdx; }
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int  rowCountForTest() const { return _rows; }
private:
  void rebuild();
  MessagesService* _svc = nullptr;
  ConvoKey         _key{};
  int              _idx = 0;
  ScrollText       _text;
  int              _rows = 0;
};

MessagePathApplet& messagePathApplet();

}  // namespace mishmesh
