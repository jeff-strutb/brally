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

# TRACKS/*.TRK are the actual game world: raw big-endian N64 memory images with
# a 0x230-byte header, sitting as plain files on the disc rather than inside any
# archive. See port/include/br_track.h.
#
# Two are pulled: race.trk is the smallest (0.9 MB) and desert.trk is the one
# with a large instance array, so between them the loader's paths are covered.
# The .hnt sidecars are tiny and exercise the ASCII hint parser -- coast.hnt is
# the only one on the disc with more than a first line.
mkdir -p testdata/tracks
for f in RACE.TRK DESERT.TRK DESERT.HNT COAST.HNT; do
  out="testdata/tracks/$(echo "$f" | tr 'A-Z' 'a-z')"
  if [ ! -f "$out" ]; then
    python3 tools/extract_iso.py --extract-path "$BIN" "TRACKS/$f" "$out"
  else
    echo "assets: $out already present"
  fi
done

# CARS/*.RCA are the car definitions, and one field in them is load-bearing for
# the physics: the four floats at +0xC8 are the car's collision box, which
# 0x1006FD90 copies into body+0x1DC..+0x1E8. Without them two of the three box
# extents are zero, 0x10067C30's reciprocals are infinite and the whole
# OBB-versus-triangle system classifies every triangle out -- i.e. no
# car-versus-track collision at all. See port/include/br_cardata.h.
#
# All sixteen are pulled: they are small (46-87 KB each, 1.2 MB in total) and
# port/tests/test_collresp.c checks that every one of them yields a usable box,
# which is what makes "the loader works" mean more than "car 0 parses".
mkdir -p testdata/cars
ncar=0; nbadcar=0
for f in CE ES NS RS SP PS M3 IP LD HM MT CU BB PJ TR MN; do
  out="testdata/cars/$(echo "$f" | tr 'A-Z' 'a-z').rca"
  if [ -f "$out" ]; then
    ncar=$((ncar+1))
  elif python3 tools/extract_iso.py --extract-path "$BIN" "CARS/$f.RCA" "$out" \
         >/dev/null 2>&1 && [ -s "$out" ]; then
    ncar=$((ncar+1))
  else
    rm -f "$out"; nbadcar=$((nbadcar+1))
  fi
done
echo "assets: $ncar car definitions in testdata/cars ($nbadcar missing)"
[ "$nbadcar" -gt 0 ] && echo "        (a missing .rca makes the box tests SKIP, not pass)"

# The SFX directory LISTING. port/tests/test_br_sfx.c checks that every file
# name the sound bank can build is a real file and that the bank accounts for
# all 73 of them -- which needs the names and nothing else, so this stays a
# few hundred bytes even though the audio itself is pulled just below.
if [ ! -f testdata/sfx.txt ]; then
  python3 tools/extract_iso.py --list "$BIN" 2>/dev/null \
    | sed -n 's|^\([Ss][Ff][Xx]/[A-Za-z0-9_.-]*\.[Ww][Aa][Vv]\).*|\1|p' \
    | tr 'A-Z' 'a-z' | sort -u > testdata/sfx.txt
  echo "assets: listed $(wc -l < testdata/sfx.txt | tr -d ' ') sfx names"
else
  echo "assets: testdata/sfx.txt already present"
fi

# The SFX audio itself. The sound backend (port/src/br_mix.c) needs real PCM
# to mix, and port/tests/test_br_sfxout.c renders the bank to a .wav it then
# asserts against. 73 files, ~6 MB total -- small enough to take whole, and
# taking whole is what makes "the bank reaches every file" checkable at the
# sample level rather than only at the filename level.
#
# Names come from testdata/sfx.txt, so this cannot invent a file the listing
# does not have. Per-file failures are counted and reported rather than
# aborting: a disc that is missing one .wav must SKIP with a reason, not look
# like a clean extraction (asset policy, README).
if [ ! -d testdata/sfx ]; then
  mkdir -p testdata/sfx
  nok=0; nbad=0
  while read -r f; do
    [ -n "$f" ] || continue
    out="testdata/sfx/$(basename "$f")"
    # The ISO stores upper case; --extract-path matches the on-disc spelling.
    up=$(echo "$f" | tr 'a-z' 'A-Z')
    if python3 tools/extract_iso.py --extract-path "$BIN" "$up" "$out" >/dev/null 2>&1 \
       && [ -s "$out" ]; then
      nok=$((nok+1))
    else
      rm -f "$out"; nbad=$((nbad+1))
    fi
  done < testdata/sfx.txt
  echo "assets: extracted $nok sfx wavs ($nbad missing)"
  [ "$nbad" -gt 0 ] && echo "        (a partial SFX/ makes the audio suites SKIP, not pass)"
else
  echo "assets: testdata/sfx already present ($(ls testdata/sfx | wc -l | tr -d ' ') wavs)"
fi

# The menu sprite sheets. br_uispr.c's 145-entry table names these; without
# them the chrome can only be drawn as placeholder rectangles.
if [ ! -d testdata/images ]; then
  mkdir -p testdata/images
  python3 tools/extract_iso.py --list "$BIN" 2>/dev/null \
    | sed -n 's/^\([Ii]mages\/[A-Za-z0-9_.-]*\.[Bb][Mm][Pp]\).*/\1/p' \
    | while read -r f; do
        out="testdata/images/$(basename "$f" | tr 'A-Z' 'a-z')"
        python3 tools/extract_iso.py --extract-path "$BIN" "$f" "$out" >/dev/null 2>&1
      done
  echo "assets: extracted $(ls testdata/images | wc -l | tr -d ' ') menu sprites"
else
  echo "assets: testdata/images already present"
fi
