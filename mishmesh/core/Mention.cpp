// mishmesh/core/Mention.cpp
#include <mishmesh/core/Mention.h>
#include <ctype.h>
#include <string.h>

namespace mishmesh {

bool textMentions(const char* text, const char* name) {
  if (!text || !name || !name[0]) return false;
  size_t nlen = strlen(name);
  for (const char* p = text; *p; ++p) {
    if (*p != '@') continue;
    const char* a = p + 1;
    bool bracketed = (*a == '[');            // @[name] variant (the app's format)
    if (bracketed) ++a;
    size_t k = 0;
    while (k < nlen && a[k] &&
           tolower((unsigned char)a[k]) == tolower((unsigned char)name[k])) ++k;
    if (k != nlen) continue;                 // didn't match the full name
    char after = a[nlen];
    // @[name] must close with ']'; bare @name needs a non-alphanumeric boundary.
    // Bracketed form also lets the name carry spaces (e.g. "@[John Doe]").
    if (bracketed ? (after == ']')
                  : (after == '\0' || !isalnum((unsigned char)after))) return true;
  }
  return false;
}

}  // namespace mishmesh
