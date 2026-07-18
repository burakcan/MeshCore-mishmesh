#!/usr/bin/env python3
# Generator for the EmojiMania overlay. Slices
# your bw sheet into a 9x9 1-bit mcufont BDF -> Emoji.c atlas and emits
# EmojiMap.inc (Unicode key -> atlas glyph). Native 9x9, no resize.
#
#   python3 build_emoji_png.py         # -> emoji-local/{Emoji.bdf, EmojiMap.inc} + copies Emoji.cpp
#   <ENC> import_bdf Emoji.bdf ; <ENC> bwfont_export Emoji.dat   # -> Emoji.c
import shutil
from pathlib import Path
from PIL import Image

HERE = Path(__file__).parent            # emoji-tools/ (committed)
OUT = HERE.parent / "emoji-local"       # gitignored: sheet in, generated art out
SHEET = OUT / "EmojiMania_bw.png"
SIZE = 9   # native glyph size (px); glyphs are stored 1:1 at this size

# (atlas_cp, [unicode codepoints it renders], x, y)
# x,y are the top-left of the 9x9 cell in the sheet.
EMOJI = [
    # Grouped: each glyph also catches close variants that have no dedicated cell
    # (and color-only variants, identical in 1-bit). Extra codepoints just render
    # the nearest glyph instead of a block placeholder.
    (0xE100, [0x1F600, 0x1F603, 0x1F601],          12,  34),  # grinning (+ open, beaming)
    (0xE101, [0x1F604, 0x1F606, 0x1F605],          34,  34),  # big grin (+ squint laugh, sweat)
    (0xE102, [0x1F602, 0x1F923],                   89,  34),  # tears of joy (+ rofl)
    (0xE103, [0x1F642, 0x1F643],                   67,  45),  # slight smile (+ upside-down)
    (0xE104, [0x1F60A, 0x1F60C, 0x263A],           45,  45),  # smiling/blush (+ relieved, white)
    (0xE105, [0x1F609],                            89,  45),  # wink
    (0xE106, [0x1F60D, 0x1F970],                   23,  56),  # heart eyes (+ smiling hearts)
    (0xE107, [0x1F618, 0x1F617, 0x1F619, 0x1F61A], 45,  56),  # kiss (+ kissing variants)
    (0xE108, [0x1F61C],                            34,  67),  # winking tongue
    (0xE109, [0x1F61B, 0x1F61D],                   45,  67),  # tongue (+ squinting tongue)
    (0xE10A, [0x1F60E],                            89,  67),  # sunglasses
    (0xE10B, [0x1F914],                            23, 122),  # thinking
    (0xE10C, [0x1F610, 0x1F611, 0x1F636],          45, 133),  # neutral (+ expressionless, no-mouth)
    (0xE10D, [0x1F641, 0x1F61E, 0x1F614],          23,  89),  # sad (+ disappointed, pensive)
    (0xE10E, [0x1F622, 0x1F625],                   12, 100),  # crying (+ sad-but-relieved)
    (0xE10F, [0x1F62D],                            23, 100),  # loudly crying
    (0xE110, [0x1F621, 0x1F620, 0x1F624],          56, 100),  # angry (+ pouting, huffing)
    (0xE111, [0x1F97A, 0x1F979],                   89, 100),  # pleading (+ holding back tears)
    (0xE112, [0x1F634, 0x1F62A],                   78, 144),  # sleeping (+ sleepy)
    (0xE113, [0x1F44D],                            34, 210),  # thumbs up
    (0xE114, [0x1F44E],                            45, 210),  # thumbs down
    (0xE115, [0x1F44F],                            12, 210),  # clapping
    (0xE116, [0x1F64F],                            45, 254),  # folded hands
    (0xE117, [0x1F44B],                            45, 243),  # waving hand
    (0xE118, [0x1F4AA],                            89, 243),  # flexed biceps
    (0xE119, [0x2764, 0x2665, 0x1F9E1, 0x1F49B, 0x1F49A, 0x1F499, 0x1F49C,
              0x1F5A4, 0x1F90D, 0x1F90E, 0x1F493, 0x1F495, 0x1F496, 0x1F497],
                                                  606,  34),  # heart (+ colors/variants, mono-identical)
    (0xE11A, [0x1F525],                           144, 276),  # fire
    (0xE11B, [0x2B50, 0x1F31F],                   166, 265),  # star (+ glowing star)
    (0xE11C, [0x1F389, 0x1F38A],                  507, 232),  # party popper (+ confetti ball)
    (0xE11D, [0x1F4AF],                           683, 133),  # hundred
    (0xE11E, [0x1F62E, 0x1F62F, 0x1F632],          45, 144),  # surprised/open mouth (+ hushed, astonished)
    (0xE11F, [0x1F64B],                            89, 463),  # person raising hand (base; gender ZWJ variants below)
]

# Rendered as nothing (advance 0) so trailing modifiers don't show a block:
# variation selectors, ZWJ, skin tones, and gender signs (the male/female/
# transgender-sign tail of gendered ZWJ sequences -- base + ZWJ + gender sign
# + VS16 -- so only the base glyph shows).
ZERO_WIDTH = [0x200D, 0xFE0E, 0xFE0F, 0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF,
              0x2640, 0x2642, 0x26A7]


def bdf_glyph(name, cp, cell):
    px = cell.load()
    nbytes = (SIZE + 7) // 8
    field = nbytes * 8
    fmt = "%%0%dX" % (nbytes * 2)
    rows = []
    for y in range(SIZE):
        bits = 0
        for x in range(SIZE):
            if px[x, y] > 127:            # sheet is white glyph on black bg
                bits |= 1 << (field - 1 - x)
        rows.append(fmt % bits)
    return (f"STARTCHAR {name}\nENCODING {cp}\nSWIDTH 1000 0\nDWIDTH {SIZE} 0\n"
            f"BBX {SIZE} {SIZE} 0 0\nBITMAP\n" + "\n".join(rows) + "\nENDCHAR\n")


def main():
    sheet = Image.open(SHEET).convert("L")
    glyphs = []
    for cp, _keys, x, y in EMOJI:
        cell = sheet.crop((x, y, x + SIZE, y + SIZE))
        glyphs.append(bdf_glyph("e%04X" % cp, cp, cell))
    hdr = (f"STARTFONT 2.1\nFONT emoji\nSIZE {SIZE} 75 75\n"
           f"FONTBOUNDINGBOX {SIZE} {SIZE} 0 0\nSTARTPROPERTIES 2\n"
           f"FONT_ASCENT {SIZE}\nFONT_DESCENT 0\nENDPROPERTIES\nCHARS {len(glyphs)}\n")
    (OUT / "Emoji.bdf").write_text(hdr + "".join(glyphs) + "ENDFONT\n")

    entries, seen = [], {}
    for cp, keys, _x, _y in EMOJI:
        for u in keys:
            k = u & 0xFFFF
            assert k not in seen, f"key collision 0x{k:04X}: {hex(u)} vs {hex(seen[k])}"
            seen[k] = u
            entries.append((k, cp))
    entries.sort()
    zw = sorted({u & 0xFFFF for u in ZERO_WIDTH})
    out = ["// Generated by build_emoji_png.py -- LOCAL, do not commit.",
           "struct EmojiEntry { uint16_t key; uint16_t glyph; };",
           "static const EmojiEntry kEmojiMap[] = {"]
    out += [f"  {{0x{k:04X}, 0x{a:04X}}}," for k, a in entries]
    out += ["};", "static const uint16_t kEmojiZeroWidth[] = {",
            "  " + ", ".join(f"0x{z:04X}" for z in zw) + ",", "};"]
    # Picker catalog: the primary (canonical) codepoint of each distinct glyph,
    # in table order (faces -> hands -> symbols). Full Unicode (uint32_t).
    catalog = [keys[0] for _cp, keys, _x, _y in EMOJI]
    out += ["static const uint32_t kEmojiCatalog[] = {",
            "  " + ", ".join(f"0x{c:X}" for c in catalog) + ",", "};"]
    (OUT / "EmojiMap.inc").write_text("\n".join(out) + "\n")
    shutil.copy(HERE / "Emoji.cpp", OUT / "Emoji.cpp")
    print(f"wrote Emoji.bdf ({len(glyphs)} glyphs) + EmojiMap.inc ({len(entries)} keys)"
          f" + copied Emoji.cpp into emoji-local/")


main()
