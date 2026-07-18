#!/usr/bin/env bash
# Maintainer-only: packages the generated emoji-local/ files (the art) into the
# EMOJI_OVERLAY_B64 value for GitHub Actions. This script is committed; the files
# it tars are gitignored. Re-run after regenerating Emoji.c/EmojiMap.inc, then
# update the repo secret: Settings -> Secrets and variables -> Actions -> EMOJI_OVERLAY_B64.
set -euo pipefail
cd "$(dirname "$0")/../emoji-local"
payload=$(tar czf - Emoji.c EmojiMap.inc Emoji.cpp | base64)
if command -v pbcopy >/dev/null 2>&1; then
  printf '%s' "$payload" | pbcopy
  echo "EMOJI_OVERLAY_B64 copied to clipboard ($(printf '%s' "$payload" | wc -c | tr -d ' ') bytes)."
else
  printf '%s\n' "$payload"
fi
