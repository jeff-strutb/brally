#!/bin/sh
# Run Wine for the matching build.
#
# Everything is kept inside the repo: the Wine binary that setup.sh unpacked
# into tools/wine/, and a Wine prefix under build/ rather than ~/.wine.  A
# system-wide Wine on PATH is used only as a fallback, so a machine that has
# never installed Wine still works and one that has isn't disturbed.
ROOT=$(cd "$(dirname "$0")/.." && pwd)

LOCAL_WINE="$ROOT/tools/wine/Wine Stable.app/Contents/Resources/wine/bin/wine"

if [ -x "$LOCAL_WINE" ]; then
    WINE="$LOCAL_WINE"
elif command -v wine >/dev/null 2>&1; then
    WINE=$(command -v wine)
else
    echo "wine not found -- run: sh setup.sh" >&2
    exit 1
fi

# A repo-local prefix keeps the host's ~/.wine untouched, and -all silences the
# Vulkan/MoltenVK banner Wine prints over every single cl.exe invocation.
export WINEPREFIX="${WINEPREFIX:-$ROOT/build/wineprefix}"
export WINEDEBUG="${WINEDEBUG:--all}"

exec "$WINE" "$@"
