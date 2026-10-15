#!/bin/sh
# music.sh DIR: Boss Rally's soundtrack, its CD audio tracks 2..13 as FLAC
# (track02.flac ...), into DIR: the PC's music of the Remaster Options.
# From the Boss Rally port's extract when it has one, else from the disc
# image (--bin, with its .cue beside it) by tools/brally/extract_cdaudio.py,
# once (build/remastered/extract/music/cd).  Without either it says so and
# leaves DIR empty: the PC's music is then silence.
#   env: BIN  the disc image (default reference/brally/BossRally.BIN)
set -e
cd "$(dirname "$0")/../.."
DIR=$1
[ -n "$DIR" ] || { echo "usage: music.sh DIR" >&2; exit 2; }
BIN=${BIN:-reference/brally/BossRally.BIN}
PY=.venv/bin/python
[ -x $PY ] || PY=python3
SRC=
for d in build/brally/macos-metal/extract/music/cd build/remastered/extract/music/cd; do
    [ -f $d/track02.flac ] && { SRC=$d; break; }
done
if [ -z "$SRC" ]; then
    CUE=
    for c in "${BIN%.*}.cue" "${BIN%.*}.CUE"; do
        [ -f "$c" ] && CUE=$c && break
    done
    if [ -f "$BIN" ] && [ -n "$CUE" ]; then
        SRC=build/remastered/extract/music/cd
        mkdir -p $SRC
        echo "music: CD audio <- $CUE"
        $PY tools/brally/extract_cdaudio.py -q "$CUE" $SRC
    else
        echo "music: no Boss Rally CD audio (no extract, no $BIN with its .cue): the PC's music is silent" >&2
    fi
fi
rm -rf "$DIR"
mkdir -p "$DIR"
[ -n "$SRC" ] && cp $SRC/track*.flac "$DIR/"
exit 0
