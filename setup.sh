#!/bin/sh
# setup.sh -- set up the matching build environment.
#
# Everything this script produces lives inside the repo.  Nothing is installed
# onto the host: no package manager, no /Applications, no ~/.wine.  A fresh
# clone plus this script plus the two disc images under reference/ is a
# complete matching build, and deleting tools/wine/ and tools/msvc5/ puts the
# machine back exactly as it was.
#
# Two pieces get staged:
#
#   Wine        Downloaded as a portable macOS build and unpacked into
#               tools/wine/.  Pinned to a version and checksummed, because the
#               whole point of a matching build is that the toolchain does not
#               drift underneath it.
#
#   MSVC 5.0    Copied out of the Visual C++ 5.0 disc image in reference/.
#               The compiler is proprietary and cannot be downloaded, but the
#               image is right there, so there is no reason to make a human
#               copy files by hand.
set -e

ROOT=$(cd "$(dirname "$0")" && pwd)
cd "$ROOT"

echo "=== Boss Rally matching build setup ==="
echo ""

# ---- Wine ----------------------------------------------------------------
# Pinned deliberately.  This is the exact build the current match counts were
# produced with; bumping it means re-verifying every matched function.
WINE_VERSION="11.0_1"
WINE_URL="https://github.com/Gcenx/macOS_Wine_builds/releases/download/11.0_1/wine-stable-11.0_1-osx64.tar.xz"
WINE_SHA256="b50dc50ec7f41d58b115a6b685d4d1315ba3c797bd3aa0f49213f2703cb82388"

WINE_DIR="tools/wine"
WINE_BIN="$WINE_DIR/Wine Stable.app/Contents/Resources/wine/bin/wine"

if [ -x "$WINE_BIN" ]; then
    echo "[ok] wine $WINE_VERSION already staged in $WINE_DIR/"
else
    echo "[fetch] Wine $WINE_VERSION (185 MB) -> $WINE_DIR/"
    mkdir -p "$WINE_DIR"
    TARBALL="$WINE_DIR/wine-$WINE_VERSION.tar.xz"

    if [ ! -f "$TARBALL" ]; then
        curl -L --fail --progress-bar -o "$TARBALL.part" "$WINE_URL" || {
            echo "FAILED to download Wine from:"
            echo "  $WINE_URL"
            rm -f "$TARBALL.part"
            exit 1
        }
        mv "$TARBALL.part" "$TARBALL"
    fi

    echo "[verify] checksum..."
    GOT=$(shasum -a 256 "$TARBALL" | cut -d' ' -f1)
    if [ "$GOT" != "$WINE_SHA256" ]; then
        echo "CHECKSUM MISMATCH -- refusing to unpack."
        echo "  expected $WINE_SHA256"
        echo "  got      $GOT"
        rm -f "$TARBALL"
        exit 1
    fi

    echo "[unpack] ..."
    tar -xJf "$TARBALL" -C "$WINE_DIR"
    rm -f "$TARBALL"

    # Downloaded archives carry the quarantine flag; Wine's own helper binaries
    # get blocked by Gatekeeper without this.
    xattr -dr com.apple.quarantine "$WINE_DIR" 2>/dev/null || true

    if [ -x "$WINE_BIN" ]; then
        echo "[ok] wine staged in $WINE_DIR/"
    else
        echo "[fail] unpacked, but no wine binary at:"
        echo "       $WINE_BIN"
        exit 1
    fi
fi

# The Wine build is x86_64; on Apple Silicon it runs under Rosetta 2.
if [ "$(uname -m)" = "arm64" ] && ! /usr/bin/pgrep -q oahd 2>/dev/null; then
    if ! /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
        echo "[warn] Rosetta 2 does not appear to be available.  Wine is an"
        echo "       x86_64 build and needs it.  Install with:"
        echo "         softwareupdate --install-rosetta --agree-to-license"
    fi
fi

echo ""

# ---- MSVC 5.0, straight off the disc image -------------------------------
MSVC_DIR="tools/msvc5"
VC_ISO="reference/msvc/VCPP-5.00.iso"

find_cl() {
    for cand in "$MSVC_DIR/bin/cl.exe" "$MSVC_DIR/cl.exe"; do
        [ -f "$cand" ] && { echo "$cand"; return 0; }
    done
    return 1
}

CL_FOUND=$(find_cl || true)

if [ -n "$CL_FOUND" ]; then
    echo "[ok] MSVC 5.0 compiler found at $CL_FOUND"
elif [ ! -f "$VC_ISO" ]; then
    echo "[need] MSVC 5.0, and no disc image at $VC_ISO"
    echo ""
    echo "  MSVC 5.0 (Visual Studio 97) is proprietary and cannot be"
    echo "  downloaded.  Put a Visual C++ 5.0 CD image there and re-run,"
    echo "  or stage the files by hand into $MSVC_DIR/bin, include, lib."
else
    echo "[extract] MSVC 5.0 from $VC_ISO"

    MNT=$(hdiutil attach -nobrowse -readonly "$VC_ISO" | awk '/\/Volumes\//{print $NF; exit}')
    if [ -z "$MNT" ]; then
        echo "[fail] could not mount $VC_ISO"
        exit 1
    fi
    # Always give the image back, however this script exits.
    trap 'hdiutil detach "$MNT" -quiet 2>/dev/null || true' EXIT
    echo "       mounted at $MNT"

    VC="$MNT/DEVSTUDIO/VC"
    if [ ! -d "$VC/BIN" ]; then
        echo "[fail] $MNT does not look like a Visual C++ 5.0 disc"
        echo "       (expected DEVSTUDIO/VC/BIN)"
        exit 1
    fi

    mkdir -p "$MSVC_DIR/bin" "$MSVC_DIR/include" "$MSVC_DIR/lib"

    echo "       bin/ ..."
    cp -f "$VC"/BIN/* "$MSVC_DIR/bin/" 2>/dev/null || true
    # cl.exe shells out to the real compiler passes and the PDB writer; the
    # PDB writer lives with the shared IDE, not with the compiler.
    for extra in "$MNT/DEVSTUDIO/SHAREDIDE/BIN/MSPDB50.DLL"; do
        [ -f "$extra" ] && cp -f "$extra" "$MSVC_DIR/bin/"
    done

    echo "       include/ ..."
    cp -Rf "$VC"/INCLUDE/* "$MSVC_DIR/include/" 2>/dev/null || true

    echo "       lib/ ..."
    cp -Rf "$VC"/LIB/* "$MSVC_DIR/lib/" 2>/dev/null || true

    chmod -R u+rw "$MSVC_DIR" 2>/dev/null || true

    hdiutil detach "$MNT" -quiet 2>/dev/null || true
    trap - EXIT

    CL_FOUND=$(find_cl || true)
    if [ -n "$CL_FOUND" ]; then
        echo "[ok] MSVC 5.0 staged: $(ls "$MSVC_DIR/bin" | wc -l | tr -d ' ') bin, \
$(ls "$MSVC_DIR/include" | wc -l | tr -d ' ') include, \
$(ls "$MSVC_DIR/lib" | wc -l | tr -d ' ') lib"
    else
        echo "[fail] extraction finished but no cl.exe landed in $MSVC_DIR/bin"
        exit 1
    fi
fi

echo ""

# ---- Original function bytes ---------------------------------------------
if [ ! -d build/match/orig ] || [ -z "$(ls build/match/orig/ 2>/dev/null)" ]; then
    echo "[extract] original function bytes from the shipped game binary..."
    mkdir -p build/match/orig
    python3 tools/extract_funcs.py orig/BRD3D.dll config/functions.csv build/match/orig/
else
    echo "[ok] original function bytes extracted ($(ls build/match/orig/*.bin 2>/dev/null | wc -l | tr -d ' ') functions)"
fi

echo ""

# ---- Keep the staged toolchain out of git --------------------------------
for path in "tools/msvc5/" "tools/wine/"; do
    grep -qxF "$path" .gitignore 2>/dev/null || {
        echo "$path" >> .gitignore
        echo "[ok] added $path to .gitignore"
    }
done

# ---- Verify ---------------------------------------------------------------
echo ""
echo "=== Status ==="
[ -x "$WINE_BIN" ] && echo "  Wine:      $WINE_VERSION (in repo)" \
                   || echo "  Wine:      MISSING"
[ -n "$CL_FOUND" ] && echo "  cl.exe:    $CL_FOUND" \
                   || echo "  cl.exe:    MISSING (see above)"
[ -d build/match/orig ] && echo "  Originals: extracted" \
                        || echo "  Originals: not extracted"
echo ""

if [ -x "$WINE_BIN" ] && [ -n "$CL_FOUND" ]; then
    echo "Ready.  Run:  sh build_match.sh"
else
    echo "Not ready -- see above."
fi
