#!/bin/sh
# pin.sh: the PlayStation build compiles the native port as it stood at the
# tag tgrally-n64-layout (game memory laid out as the N64's, libultra's
# names), not the native port of today, which has moved on to native memory
# and drawing.  Sourced by build.sh and link.sh from the repository root:
# makes build/tgrally/ps1-pin/root, the repository with ports/tgrally taken
# from the tag and everything else linked, and runs the calling script there.
PIN_TAG=tgrally-n64-layout
if [ -z "$TGR_PS1_PINNED" ]; then
  REPO=$(pwd)
  ROOT=$REPO/build/tgrally/ps1-pin/root
  STAMP=$ROOT/.pin
  rev=$(git rev-parse "$PIN_TAG^{commit}") || { echo "pin: no tag $PIN_TAG" >&2; exit 2; }
  if [ "$(cat "$STAMP" 2>/dev/null)" != "$rev" ]; then
    rm -rf "$ROOT"
    mkdir -p "$ROOT/ports"
    git archive "$rev" ports/tgrally | tar -x -C "$ROOT"
    for e in $(ls -A "$REPO"); do
      case "$e" in ports|.git) continue;; esac
      ln -s "$REPO/$e" "$ROOT/$e"
    done
    for e in $(ls -A "$REPO/ports"); do
      [ "$e" = tgrally ] || ln -s "$REPO/ports/$e" "$ROOT/ports/$e"
    done
    echo "$rev" > "$STAMP"
  fi
  export TGR_PS1_PINNED=1
  cd "$ROOT" && exec sh "$ROOT/ports/tgrally-ps1/$(basename "$0")" "$@"
fi
