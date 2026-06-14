// mishmesh/core/Mention.h
#pragma once

namespace mishmesh {

// True if `text` mentions `name` written as "@name" with a non-alphanumeric
// boundary after the name (case-insensitive). Empty/null inputs never match.
// No @all/@everyone handling - only the literal handle.
bool textMentions(const char* text, const char* name);

}  // namespace mishmesh
