#include <mishmesh/core/StrUtil.h>
#include <string.h>

namespace mishmesh {

void copyStr(char* dst, size_t cap, const char* src) {
  if (!cap) return;
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = 0;
}

}  // namespace mishmesh
