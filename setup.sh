#!/bin/sh
# setup.sh -- set up the matching build environment.
#
# Everything this script produces lives inside the repo.  Nothing is installed
# onto the host: no package manager, no /Applications, no ~/.wine.  A fresh
# clone plus this script plus the disc images under reference/ is a complete
# matching build, and deleting tools/wine/, tools/msvc5/ and orig/ puts the
# machine back exactly as it was.
#
# Four pieces get staged:
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
#
#   orig/       Game binaries pulled out of the Boss Rally BIN/CUE.  The match
#               target is BRD3D.dll; BRGlide.dll is the renderer reference.
#               Do not copy these by hand.
#
#   N64 music   XM modules rendered out of the Top Gear Rally ROM, if one is
#               sitting under reference/tgrally/.  Optional: the matching
#               build does not need it.
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

# ---- Game originals, off the BIN/CUE and the N64 ROM ---------------------
# Hashes of the dumps this tree was matched against.  Documented in README.md
# so a builder can source the same images; checked here so a different dump
# does not silently become the match target.
BRALLY_BIN_MD5="31c64f9b1e09788c2dfc384b44af8f6c"
BRALLY_CUE_MD5="a48a4a5860558177c3041afee57e03c9"
TGR_ROM_MD5="6f7030284b6bc84a49e07da864526b52"

md5_of() {
    md5 -q "$1"
}

warn_md5() {
    _path="$1"
    _want="$2"
    _label="$3"
    _got=$(md5_of "$_path")
    if [ "$_got" != "$_want" ]; then
        echo "[warn] $_label MD5 does not match the dump this tree was built against."
        echo "       expected $_want"
        echo "       got      $_got"
        echo "       (extraction continues; matching results may not agree)"
        return 1
    fi
    echo "[ok] $_label MD5 $_want"
    return 0
}

BRALLY_BIN=""
for c in reference/brally/BossRally.BIN reference/brally/BossRally.bin; do
    [ -f "$c" ] && BRALLY_BIN="$c" && break
done
BRALLY_CUE=""
if [ -n "$BRALLY_BIN" ]; then
    for c in "${BRALLY_BIN%.BIN}.cue" "${BRALLY_BIN%.BIN}.CUE" "${BRALLY_BIN%.bin}.cue"; do
        [ -f "$c" ] && BRALLY_CUE="$c" && break
    done
fi

TGR_ROM=""
for c in "reference/tgrally/Top Gear Rally (USA).z64"; do
    [ -f "$c" ] && TGR_ROM="$c" && break
done
if [ -z "$TGR_ROM" ]; then
    for c in reference/tgrally/*.z64 reference/tgrally/*.n64 reference/tgrally/*.v64; do
        [ -f "$c" ] && TGR_ROM="$c" && break
    done
fi

need_orig=0
for f in orig/BRD3D.dll orig/BRGlide.dll; do
    [ -f "$f" ] || need_orig=1
done

if [ "$need_orig" -eq 0 ]; then
    echo "[ok] orig/ already staged"
elif [ -z "$BRALLY_BIN" ]; then
    echo "[need] orig/BRD3D.dll and orig/BRGlide.dll, and no disc image at"
    echo "       reference/brally/BossRally.BIN"
    echo ""
    echo "  Put the retail Boss Rally BIN/CUE there (MD5 of the BIN this tree"
    echo "  was matched against: $BRALLY_BIN_MD5) and re-run.  setup.sh"
    echo "  extracts the binaries; do not copy them into orig/ by hand."
    exit 1
else
    echo "[extract] orig/ from $BRALLY_BIN"
    warn_md5 "$BRALLY_BIN" "$BRALLY_BIN_MD5" "BossRally.BIN" || true
    if [ -n "$BRALLY_CUE" ]; then
        warn_md5 "$BRALLY_CUE" "$BRALLY_CUE_MD5" "BossRally.cue" || true
    else
        echo "[warn] no .cue beside $BRALLY_BIN (binaries still extract; CD audio will not)"
    fi

    mkdir -p orig
    # src on the disc -> dest under orig/.  Names on the right are what the
    # rest of the tree opens.
    while read -r src dst; do
        [ -n "$src" ] || continue
        if [ -f "orig/$dst" ]; then
            continue
        fi
        python3 tools/extract_iso.py --extract-path "$BRALLY_BIN" "$src" "orig/$dst" >/dev/null
    done <<'EOF'
BRD3D.dll BRD3D.dll
BRGlide.dll BRGlide.dll
BRally.exe BRally.exe
Boot.exe Boot.exe
BossRally.exe BossRally.exe
SetVideo.exe SetVideo.exe
Remove.exe REMOVE.EXE
SETUP.EXE SETUP.EXE
_ISDEL.EXE _ISDEL.EXE
_SETUP.DLL _SETUP.DLL
EOF

    if [ ! -f orig/BRD3D.dll ] || [ ! -f orig/BRGlide.dll ]; then
        echo "[fail] extraction finished but orig/BRD3D.dll or orig/BRGlide.dll is missing"
        exit 1
    fi
    echo "[ok] orig/ staged from the disc"
fi

echo ""

# Testdata assets (tracks, cars, sprites, CD audio).  Idempotent: skipped
# once testdata/strings.txt is already there.
if [ -n "$BRALLY_BIN" ]; then
    if [ ! -f testdata/strings.txt ]; then
        echo "[extract] testdata/ from $BRALLY_BIN"
        tools/extract_assets.sh "$BRALLY_BIN"
    else
        echo "[ok] testdata/ already present"
    fi
fi

echo ""

if [ -n "$TGR_ROM" ]; then
    echo "[ok] Top Gear Rally ROM at $TGR_ROM"
    warn_md5 "$TGR_ROM" "$TGR_ROM_MD5" "$(basename "$TGR_ROM")" || true
    if [ -f testdata/music_xm/xm.manifest.json ]; then
        echo "[ok] N64 soundtrack already extracted"
    elif command -v ffmpeg >/dev/null 2>&1; then
        echo "[extract] N64 soundtrack from $TGR_ROM"
        python3 tools/extract_xm.py "$TGR_ROM" testdata/music_xm
    else
        echo "[skip] N64 soundtrack: ffmpeg not on PATH (FLAC encoder)."
        echo "       Matching does not need it; the port will have no N64 music."
    fi
else
    echo "[skip] no Top Gear Rally ROM under reference/tgrally/ (optional)"
fi

echo ""

# ---- Original function bytes ---------------------------------------------
if [ ! -f orig/BRD3D.dll ]; then
    echo "[fail] orig/BRD3D.dll is missing -- cannot extract function bytes"
    exit 1
fi
if [ ! -d build/match/orig ] || [ -z "$(ls build/match/orig/ 2>/dev/null)" ]; then
    echo "[extract] original function bytes from the shipped game binary..."
    mkdir -p build/match/orig
    python3 tools/extract_funcs.py orig/BRD3D.dll config/functions.csv build/match/orig/
    echo "[ok] original function bytes extracted ($(ls build/match/orig/*.bin 2>/dev/null | wc -l | tr -d ' ') functions)"
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
[ -f orig/BRD3D.dll ] && echo "  orig/:     BRD3D.dll staged" \
                      || echo "  orig/:     MISSING"
[ -d build/match/orig ] && echo "  Functions: extracted" \
                        || echo "  Functions: not extracted"
[ -n "$TGR_ROM" ] && echo "  TGR ROM:   $TGR_ROM" \
                  || echo "  TGR ROM:   not present (optional)"
echo ""

if [ -x "$WINE_BIN" ] && [ -n "$CL_FOUND" ]; then
    echo "Ready.  Run:  sh build_match.sh"
else
    echo "Not ready -- see above."
fi
