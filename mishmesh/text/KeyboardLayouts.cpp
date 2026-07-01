#include <mishmesh/text/KeyboardLayouts.h>
#include <string.h>

// Multi-tap letter groups follow Nokia's national-layout convention: each key
// lists its base ASCII letters first, then that key's accented variants; case is
// applied by the keypad's shift mode via the explicit `upper` arrays. All glyphs
// are covered by the mishmesh bitmap fonts (Latin-1 + synthesized Latin Extended-A).

namespace mishmesh {

#define P ".,?!"   // shared punctuation cell (index 0)

static const KbdLayout LAYOUTS[] = {
  { "EN", "English",
    { P, "abc","def","ghi","jkl","mno","pqrs","tuv","wxyz" },
    { P, "ABC","DEF","GHI","JKL","MNO","PQRS","TUV","WXYZ" } },

  { "DE", "Deutsch",
    { P, "abcä","def","ghi","jkl","mnoö","pqrsß","tuvü","wxyz" },
    { P, "ABCÄ","DEF","GHI","JKL","MNOÖ","PQRSß","TUVÜ","WXYZ" } },

  { "FR", "Français",
    { P, "abcàâæç","deféèêë","ghiîï","jkl","mnoô","pqrs","tuvùûü","wxyz" },
    { P, "ABCÀÂÆÇ","DEFÉÈÊË","GHIÎÏ","JKL","MNOÔ","PQRS","TUVÙÛÜ","WXYZ" } },

  { "ES", "Español",
    { P, "abcá","defé","ghií","jkl","mnoñó","pqrs","tuvúü","wxyz" },
    { P, "ABCÁ","DEFÉ","GHIÍ","JKL","MNOÑÓ","PQRS","TUVÚÜ","WXYZ" } },

  { "IT", "Italiano",
    { P, "abcàá","defèé","ghiìí","jkl","mnoòó","pqrs","tuvùú","wxyz" },
    { P, "ABCÀÁ","DEFÈÉ","GHIÌÍ","JKL","MNOÒÓ","PQRS","TUVÙÚ","WXYZ" } },

  { "PT", "Português",
    { P, "abcáàâãç","deféê","ghií","jkl","mnoóôõ","pqrs","tuvú","wxyz" },
    { P, "ABCÁÀÂÃÇ","DEFÉÊ","GHIÍ","JKL","MNOÓÔÕ","PQRS","TUVÚ","WXYZ" } },

  { "SV", "Svenska",
    { P, "abcåä","def","ghi","jkl","mnoö","pqrs","tuv","wxyz" },
    { P, "ABCÅÄ","DEF","GHI","JKL","MNOÖ","PQRS","TUV","WXYZ" } },

  { "NO", "Norsk",
    { P, "abcæå","def","ghi","jkl","mnoø","pqrs","tuv","wxyz" },
    { P, "ABCÆÅ","DEF","GHI","JKL","MNOØ","PQRS","TUV","WXYZ" } },

  { "TR", "Türkçe",
    { P, "abcç","def","ghıiğ","jkl","mnoö","pqrsş","tuvü","wxyz" },
    { P, "ABCÇ","DEF","GHIİĞ","JKL","MNOÖ","PQRSŞ","TUVÜ","WXYZ" } },

  { "PL", "Polski",
    { P, "abcąć","defę","ghi","jklł","mnońó","pqrsś","tuv","wxyzźż" },
    { P, "ABCĄĆ","DEFĘ","GHI","JKLŁ","MNOŃÓ","PQRSŚ","TUV","WXYZŹŻ" } },

  { "CS", "Čeština",
    { P, "abcáč","defďéě","ghií","jkl","mnoňó","pqrsřš","tuvťúů","wxyzýž" },
    { P, "ABCÁČ","DEFĎÉĚ","GHIÍ","JKL","MNOŇÓ","PQRSŘŠ","TUVŤÚŮ","WXYZÝŽ" } },

  { "HU", "Magyar",
    { P, "abcá","defé","ghií","jkl","mnoóöő","pqrs","tuvúüű","wxyz" },
    { P, "ABCÁ","DEFÉ","GHIÍ","JKL","MNOÓÖŐ","PQRS","TUVÚÜŰ","WXYZ" } },
};

#undef P

static const int LAYOUT_COUNT = (int)(sizeof(LAYOUTS) / sizeof(LAYOUTS[0]));

int kbdLayoutCount() { return LAYOUT_COUNT; }

const KbdLayout& kbdLayoutAt(int i) {
  if (i < 0 || i >= LAYOUT_COUNT) i = 0;
  return LAYOUTS[i];
}

int kbdLayoutIndexByCode(const char* code) {
  if (!code) return -1;
  for (int i = 0; i < LAYOUT_COUNT; i++)
    if (strcmp(LAYOUTS[i].code, code) == 0) return i;
  return -1;
}

}  // namespace mishmesh
