#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/widgets/StatusBar.h>
#include <string.h>

namespace mishmesh {

void setTargetFields(uint8_t* pub, char* name, size_t nameCap,
                     const uint8_t* pubKey, const char* src) {
  memcpy(pub, pubKey, 6);
  copyStr(name, nameCap, src);
}

int drawTopBar(Canvas& c, StatusBar& bar, const char* title, AppServices* app, int w) {
  bar.setTitle(title);
  bar.setBattery(app ? app->batteryMillivolts() : 0);
  int bw = 0, bh = 0; bar.measure(bw, bh);
  bar.draw(c, 0, 0, w, bh);
  return bh;
}

}  // namespace mishmesh
