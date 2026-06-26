# mishmesh emoji overlay (licensed, local-only)

How the on-device emoji rendering is set up. The **art never enters this repo** —
only the generic plumbing does. This doc is art-free process documentation.

## Why it's split

The emoji glyphs come from **EmojiMania** (© Idan Rooze, purchased). The license
permits use in your own builds but **not distribution** — and this repo is public,
so the bitmaps (in any form, incl. C arrays) must not be committed. Official
release binaries that embed the art are a *separate* act of distribution — get
the author's written permission before publishing releases that include it.

## What's where

**Committed (generic, art-free):**
- `mishmesh/core/Canvas.{h,cpp}` — `Canvas::setEmojiRenderer(font, lookup, zeroWidth)`
  plus the `mm_char` draw path and overlay-aware `textWidth`. Does nothing until an
  overlay registers; unset ⇒ behaviour identical to upstream. `kEmojiPadPx` = per-side
  padding (px) so glyphs don't butt against neighbours.
- `variants/wio-tracker-l1/platformio.ini` — build-glob `+<../mishmesh/text/emoji-local/*.c|*.cpp>`
  (empty/no-op on a fresh clone, so the dir being absent just means "no emoji").
- `test/test_mishmesh_glyphoverlay/` — tests the hook using `iconFont()` as a stand-in
  overlay (no art needed).
- `.github/workflows/build-companion-firmwares.yml` — the CI inject step (below).

**Gitignored `mishmesh/text/emoji-local/` (the art + generator — never committed):**
- `EmojiMania_bw.png` — the source sheet (local copy).
- `build_emoji_png.py` — slices 9×9 cells at fixed coords → 1-bit BDF; also emits the map.
- `Emoji.c` — generated mcufont atlas (`mf_bwfont_Emoji`); `EmojiMap.inc` — Unicode→glyph map.
- `Emoji.cpp` — self-registers the atlas into `Canvas::setEmojiRenderer` at startup
  (so no committed file references the overlay).
- `make-secret.sh` — prints/copies the CI secret value (below).
- Only `.gitkeep` from this dir is tracked (keeps the build-glob's dir valid on clones).

## Regenerating the atlas

Edit the `EMOJI` table in `build_emoji_png.py` (add/group codepoints or cells), then:

```bash
cd mishmesh/text/emoji-local
python3 build_emoji_png.py                      # -> Emoji.bdf + EmojiMap.inc
ENC=/path/to/mcufont/encoder/mcufont            # built per fonts/regenerate.sh
"$ENC" import_bdf Emoji.bdf && "$ENC" bwfont_export Emoji.dat && rm -f Emoji.dat Emoji.bdf
```

Local device build then picks it up automatically (the glob). After any change,
**update the CI secret** (below).

## GitHub Actions: official builds get emoji, forks/clones don't

Forks and fork-PRs never receive repo secrets, so they build emoji-free with no
extra logic. The release workflow injects the overlay only when the secret is set.

**One-time (and after each regen) — set the secret:**
1. Generate the value: `bash mishmesh/text/emoji-local/make-secret.sh` (copies to clipboard, ~14KB).
2. GitHub → repo **Settings → Secrets and variables → Actions → New repository secret**
   - Name: `EMOJI_OVERLAY_B64`
   - Value: paste.

**What the workflow does** (`build-companion-firmwares.yml`): a job-level env maps
the secret; an *"Inject licensed emoji overlay"* step runs only when that env is
non-empty, decoding it into `mishmesh/text/emoji-local/` before the build:

```yaml
env:
  EMOJI_OVERLAY_B64: ${{ secrets.EMOJI_OVERLAY_B64 }}
# ...
- name: Inject licensed emoji overlay (official builds only)
  if: ${{ env.EMOJI_OVERLAY_B64 != '' }}
  run: |
    mkdir -p mishmesh/text/emoji-local
    printf '%s' "$EMOJI_OVERLAY_B64" | base64 --decode | tar xz -C mishmesh/text/emoji-local
```

**Trigger:** push a `companion-*` tag, or Actions → *Build Companion Firmwares* →
Run workflow. Only the companion workflow injects (repeater/room-server don't build
the mishmesh UI; PR-check builds intentionally skip it so PR artifacts stay art-free).

The secret carries only the generated `Emoji.c` + `EmojiMap.inc` + `Emoji.cpp`
(~14KB, under GitHub's ~48KB limit); the PNG and generator stay on your machine.
