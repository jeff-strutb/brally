#!/bin/sh
# setup.sh -- set up the matching build environment.
#
# Installs Wine (for running MSVC 5.0's cl.exe on macOS) and prepares the
# tools/msvc5/ directory for the compiler files.
#
# MSVC 5.0 (Visual Studio 97) is proprietary and cannot be downloaded
# automatically.  This script creates the directory structure and tells
# you what to copy.  If you have the VC5 CD or an install on your Windows
# machine, the files are small (~15 MB total).
set -e

echo "=== Boss Rally matching build setup ==="
echo ""

# ---- Wine ----------------------------------------------------------------
if command -v wine >/dev/null 2>&1; then
    echo "[ok] wine already installed: $(wine --version 2>/dev/null || echo 'unknown version')"
else
    echo "[install] Wine..."
    # wine-stable is deprecated in Homebrew for Gatekeeper reasons only --
    # it works fine.  Install it and strip the quarantine attribute so macOS
    # doesn't block it.
    brew install --cask wine-stable 2>/dev/null || {
        echo ""
        echo "Homebrew install failed.  Trying direct download..."
        echo ""
        # Fallback: download the .pkg from WineHQ's GitHub releases.
        # This URL tracks the latest stable release.
        WINE_PKG="$TMPDIR/wine-stable.pkg"
        curl -L -o "$WINE_PKG" \
            "https://dl.winehq.org/wine-builds/macosx/pool/portable-winehq-stable-11.0~1-osx64.tar.gz" \
            2>/dev/null || {
            echo "FAILED to download Wine.  Install it manually:"
            echo "  brew install --cask wine-stable"
            echo "  # then: xattr -dr com.apple.quarantine '/Applications/Wine Stable.app'"
            exit 1
        }
        # Extract to /usr/local/opt/wine
        mkdir -p /usr/local/opt/wine
        tar xzf "$WINE_PKG" -C /usr/local/opt/wine
        rm -f "$WINE_PKG"
    }

    # Strip Gatekeeper quarantine so macOS lets it run
    WINE_APP="/Applications/Wine Stable.app"
    if [ -d "$WINE_APP" ]; then
        echo "[fix] Stripping Gatekeeper quarantine from Wine..."
        xattr -dr com.apple.quarantine "$WINE_APP" 2>/dev/null || true
    fi

    # Verify
    if command -v wine >/dev/null 2>&1; then
        echo "[ok] wine installed: $(wine --version 2>/dev/null)"
    else
        echo "[warn] wine not on PATH.  You may need to restart your shell."
        echo "       Expected at: /Applications/Wine Stable.app/Contents/Resources/wine/bin/wine"
    fi
fi

echo ""

# ---- MSVC 5.0 directory structure ----------------------------------------
MSVC_DIR="tools/msvc5"
mkdir -p "$MSVC_DIR/include" "$MSVC_DIR/lib"

if [ -f "$MSVC_DIR/cl.exe" ]; then
    echo "[ok] MSVC 5.0 compiler found at $MSVC_DIR/cl.exe"
else
    echo "[need] MSVC 5.0 compiler files."
    echo ""
    echo "  Copy these from a Visual Studio 97 / Visual C++ 5.0 install:"
    echo ""
    echo "  From VC\\bin\\:         Into $MSVC_DIR/:"
    echo "    cl.exe                  cl.exe"
    echo "    c1.exe                  c1.exe"
    echo "    c2.exe                  c2.exe"
    echo "    link.exe                link.exe"
    echo ""
    echo "  From VC\\include\\:     Into $MSVC_DIR/include/"
    echo "    (all .h files)          (the CRT/Win32 headers)"
    echo ""
    echo "  From VC\\lib\\:         Into $MSVC_DIR/lib/"
    echo "    libc.lib                libc.lib"
    echo "    kernel32.lib            kernel32.lib"
    echo "    user32.lib              user32.lib"
    echo "    gdi32.lib               gdi32.lib"
    echo "    (other .lib as needed)  ..."
    echo ""
    echo "  If you have a Windows machine with VC5 installed, the default"
    echo "  install path is:"
    echo "    C:\\Program Files\\DevStudio\\VC\\"
    echo ""
    echo "  Total size is ~15 MB.  Tar it up, scp it over, unpack here."
fi

echo ""

# ---- Extract original function bytes (if not done) -----------------------
if [ ! -d build/match/orig ] || [ -z "$(ls build/match/orig/ 2>/dev/null)" ]; then
    echo "[extract] Extracting original function bytes from BRD3D.dll..."
    mkdir -p build/match/orig
    python3 tools/extract_funcs.py orig/BRD3D.dll config/functions.csv build/match/orig/
else
    echo "[ok] Original function bytes already extracted ($(ls build/match/orig/*.bin 2>/dev/null | wc -l | tr -d ' ') functions)"
fi

echo ""

# ---- .gitignore for tools/msvc5 ------------------------------------------
if ! grep -q 'tools/msvc5' .gitignore 2>/dev/null; then
    echo "tools/msvc5/" >> .gitignore
    echo "[ok] Added tools/msvc5/ to .gitignore"
fi

echo ""

# ---- Verify ---------------------------------------------------------------
echo "=== Status ==="
command -v wine >/dev/null 2>&1 && echo "  Wine:     installed" || echo "  Wine:     NOT INSTALLED"
[ -f "$MSVC_DIR/cl.exe" ]       && echo "  cl.exe:   found"     || echo "  cl.exe:   MISSING (see above)"
[ -d build/match/orig ]         && echo "  Originals: extracted" || echo "  Originals: not extracted"
echo ""

if [ -f "$MSVC_DIR/cl.exe" ] && command -v wine >/dev/null 2>&1; then
    echo "Ready.  Run:  sh build_match.sh"
elif [ -f "$MSVC_DIR/cl.exe" ]; then
    echo "Wine missing.  Install it, then run:  sh build_match.sh"
else
    echo "Copy VC5 compiler files to $MSVC_DIR/, then run:  sh build_match.sh"
fi
