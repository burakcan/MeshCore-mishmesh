# emoji-tools (bring your own EmojiMania)

Tooling to compile the on-device emoji glyphs into a mishmesh build.
The glyphs come from the [EmojiMania](https://idanro.itch.io/emojimania) set
(copyright Idan Rooze). So this directory carries only the generator,
the registration glue, and these docs; never the bitmaps. If you do not have
EmojiMania, ignore this directory: mishmesh builds fine without it and renders a
placeholder block where an emoji would be.

## Build with emoji

The sheet and all generated output live in the sibling `../emoji-local/`
directory (which is gitignored).

1. Buy EmojiMania and save the b&w sheet as `mishmesh/text/emoji-local/EmojiMania_bw.png`.
2. Generate the atlas source and map:

   ```bash
   python3 mishmesh/text/emoji-tools/build_emoji_png.py
   ```

   This writes `emoji-local/Emoji.bdf` and `emoji-local/EmojiMap.inc`, and copies
   `Emoji.cpp` into `emoji-local/`.

3. Encode the BDF into the mcufont C atlas (build the encoder per
   `mishmesh/text/fonts/regenerate.sh`):

   ```bash
   cd mishmesh/text/emoji-local
   ENC=/path/to/mcufont/encoder/mcufont
   "$ENC" import_bdf Emoji.bdf
   "$ENC" bwfont_export Emoji.dat        # -> Emoji.c
   rm -f Emoji.dat Emoji.bdf
   ```

4. Build any mishmesh env, e.g.:

   ```bash
   sh build.sh build-firmware WioTrackerL1_companion_radio_ble_mishmesh
   ```

   The PlatformIO glob picks up `emoji-local/Emoji.c` + `Emoji.cpp`, the overlay
   self-registers, and emoji render.
