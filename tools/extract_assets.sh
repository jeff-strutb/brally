#!/bin/bash
# Extract build-time assets from media the BUILDER supplies.
#
# Nothing here is committed. The repository contains code only; the retail
# content stays with whoever owns a copy of the game. That is the project's
# asset policy and this script is the only thing that bridges it.
#
# Everything is idempotent and OPTIONAL: if the disc image is absent the build
# still succeeds and the harness falls back to visible placeholders. A missing
# asset must never look like a passing extraction.
#
# Usage: tools/extract_assets.sh [path/to/BossRally.BIN]
set -e
cd "$(dirname "$0")/.."

BIN="${1:-}"
if [ -z "$BIN" ]; then
  for c in reference/brally/BossRally.BIN \
           ../../../strutb/brally/reference/brally/BossRally.BIN; do
    [ -f "$c" ] && BIN="$c" && break
  done
fi

if [ -z "$BIN" ] || [ ! -f "$BIN" ]; then
  echo "assets: no disc image found -- skipping (the build still works;"
  echo "        the menu will show <str NNN> placeholders instead of captions)"
  exit 0
fi

mkdir -p testdata work

# BRString.dll is a RESOURCE-ONLY satellite: no code, just RT_STRING resources
# in UTF-16LE. Its loader is unported, which is why BrStrGet returned NULL for
# every id and every menu control was laid out with no text at all.
if [ ! -f testdata/strings.txt ]; then
  python3 tools/extract_iso.py "$BIN" BRSTRING.DLL work/BRString.dll
  python3 tools/extract_strings.py work/BRString.dll testdata/strings.txt
else
  echo "assets: testdata/strings.txt already present"
fi
