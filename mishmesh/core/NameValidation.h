#pragma once

namespace mishmesh {

// Node/advert name rules, matching the firmware CLI (CommonCLI::isValidName):
// non-empty and free of characters the mesh name grammar reserves.
inline bool isValidNodeName(const char* n) {
  if (!n || !n[0]) return false;               // empty not allowed
  for (const char* p = n; *p; ++p) {
    switch (*p) {
      case '[': case ']': case '\\': case ':':
      case ',': case '?': case '*':
        return false;
      default: break;
    }
  }
  return true;
}

}  // namespace mishmesh
