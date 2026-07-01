#pragma once

namespace mishmesh {

// A national multi-tap layout for the Nokia-style KeypadApplet. Group index 0 is
// the punctuation cell (".,?!"); indices 1..8 are the abc..wxyz letter cells.
// Strings are UTF-8; `upper` is stored explicitly (not derived) because Turkish
// case mapping is irregular (i<->İ, ı<->I) and future non-Latin scripts need it.
struct KbdLayout {
  const char* code;      // "EN","DE","TR" - top-right indicator + persisted value
  const char* name;      // picker row label ("English","Deutsch","Türkçe")
  const char* lower[9];  // multi-tap groups, cells 0..8, lowercase mode
  const char* upper[9];  // uppercase mode
};

int              kbdLayoutCount();
const KbdLayout& kbdLayoutAt(int i);              // out-of-range clamps to 0
int              kbdLayoutIndexByCode(const char* code);  // -1 if absent/null

}  // namespace mishmesh
