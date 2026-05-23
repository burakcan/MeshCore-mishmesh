#include <mishmesh/core/AppletRegistry.h>

namespace mishmesh {

static AppletRegistration* s_head = nullptr;

void registerApplet(AppletRegistration* reg) {
  if (reg == nullptr) return;
  reg->next = s_head;
  s_head = reg;
}

AppletRegistration* registeredApplets() {
  return s_head;
}

void resetRegistry() {
  s_head = nullptr;
}

}  // namespace mishmesh
