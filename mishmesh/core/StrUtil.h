#pragma once

#include <stddef.h>

namespace mishmesh {

// Truncating copy into a fixed buffer: strncpy + guaranteed NUL terminator, the
// `strncpy(dst, src, cap-1); dst[cap-1] = 0;` idiom that was open-coded at dozens
// of call sites. A null src copies an empty string. Pass sizeof(dst) as cap.
void copyStr(char* dst, size_t cap, const char* src);

}  // namespace mishmesh
