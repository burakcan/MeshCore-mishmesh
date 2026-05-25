#!/usr/bin/env bash
# Regenerates the bw atlases in this directory with the mcufont encoder. The
# generated *.c are committed; run this only to change faces, sizes, char sets,
# or the icon list. Requires a built mcufont encoder (see
# https://github.com/mcufont/mcufont, `make -C encoder`), curl, and python3+PIL.
#
#   ./regenerate.sh /path/to/mcufont/encoder/mcufont
set -euo pipefail
ENC="${1:?usage: regenerate.sh <path-to-mcufont-encoder>}"
cd "$(dirname "$0")"

# --- Text: Nokia Cellphone FC (freeware, vendored nokiafc22.ttf). Pixel-perfect
#     at size 8 and multiples (16, 24). ---
gen_text() {  # role size filter...
  local role="$1" sz="$2"; shift 2
  "$ENC" import_ttf nokiafc22.ttf "$sz" bw
  "$ENC" filter "nokiafc22${sz}bw.dat" "$@"
  cp "nokiafc22${sz}bw.dat" "$role.dat"
  "$ENC" bwfont_export "$role.dat"
  rm -f "nokiafc22${sz}bw.dat" "$role.dat"
}
gen_text Body  8  0x20-0xFF        # lists, status bar, hints
gen_text Title 16 0x20-0xFF        # screen titles
gen_text Num   24 0x20 0x2E-0x3A   # clock/stopwatch: space . / 0-9 :

# --- Icons: Pixelarticons (MIT) rasterised to a 12px BDF, then mcufont ---
python3 build_icons.py                # writes Icons16.bdf (downloads SVGs)
"$ENC" import_bdf Icons16.bdf
"$ENC" bwfont_export Icons16.dat
rm -f Icons16.dat Icons16.bdf

echo "Regenerated Body.c Title.c Num.c Icons16.c"
