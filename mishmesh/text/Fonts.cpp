#include <mishmesh/text/Fonts.h>
#include <mf_bwfont.h>

// Generated atlases (text/fonts/*.c). mf_bwfont_s begins with its mf_font_s,
// so the address doubles as the generic font pointer.
extern "C" {
extern const struct mf_bwfont_s mf_bwfont_Body;
extern const struct mf_bwfont_s mf_bwfont_Subtitle;
extern const struct mf_bwfont_s mf_bwfont_Title;
extern const struct mf_bwfont_s mf_bwfont_Num;
extern const struct mf_bwfont_s mf_bwfont_Icons16;
}

namespace mishmesh {

const Font* fontBody()     { return (const Font*)&mf_bwfont_Body; }
const Font* fontSubtitle() { return (const Font*)&mf_bwfont_Subtitle; }
const Font* fontTitle()    { return (const Font*)&mf_bwfont_Title; }
const Font* fontNum()      { return (const Font*)&mf_bwfont_Num; }
const Font* iconFont()     { return (const Font*)&mf_bwfont_Icons16; }

}  // namespace mishmesh
