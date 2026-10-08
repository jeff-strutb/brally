#!/bin/sh
# builder/release.sh: build Rally Builder for macOS and Windows and publish
# both on the repository's GitHub Releases page as vVERSION.
#
# VERSION is the average of the two decompilations' M2 progress (version.py).
# The release is tagged at HEAD as it is when this starts, which must
# already be on GitHub. Running it
# again at the same version replaces the files of that release.
#
# Usage: builder/release.sh [--dry-run]
set -e
cd "$(dirname "$0")/.."
PY=.venv/bin/python
[ -x $PY ] || PY=python3
DRY=0
[ "$1" = --dry-run ] && DRY=1

VERSION=$($PY builder/version.py)
TAG=v$VERSION
DIST=build/builder/dist
MAC=$DIST/RallyBuilder-$VERSION-macOS.zip
WIN=$DIST/RallyBuilder-$VERSION-Windows.zip

if [ -n "$(git status --porcelain -- builder ports)" ]; then
    echo "release: builder/ or ports/ has uncommitted changes; commit them first" >&2
    exit 1
fi
git fetch -q origin
REV=$(git rev-parse HEAD)          # the commit checked here is the one tagged, whatever lands meanwhile
if [ $DRY = 0 ] && ! git merge-base --is-ancestor $REV origin/main; then
    echo "release: HEAD is not on origin/main; push it first" >&2
    exit 1
fi

VERSION=$VERSION builder/build.sh

NOTES=$(mktemp)
$PY builder/version.py --detail | sed '$d' > $NOTES.m2
cat > $NOTES <<EOF
Rally Builder makes a native build of **Boss Rally** (PC, 1999) or **Top Gear Rally** (Nintendo 64, 1997) for your Mac or PC, from your own copy of the game. The games' data is copyrighted, so the builder does not include it: you provide your dump and the builder checks it by MD5, takes what the game needs from it, and produces a game that runs on its own. The dump is not needed once the build is done.

Version $VERSION is the decompilations' byte-exact (M2) progress, the average of the two:
\`\`\`
$(cat $NOTES.m2)
\`\`\`

### Download
- **macOS** (11 or later; Top Gear Rally needs 12. Apple silicon and Intel): \`RallyBuilder-$VERSION-macOS.zip\`. Unzip and open Rally Builder. It is not notarized: if macOS refuses to open it, Control-click it and choose Open, or allow it in System Settings, Privacy & Security.
- **Windows** (10 or later, 64-bit): \`RallyBuilder-$VERSION-Windows.zip\`. Unzip it and run \`RallyBuilder.exe\` from the unzipped folder (the \`games\` folder beside it holds the games' code). It is not signed: if SmartScreen stops it, choose More info, then Run anyway.

### What it needs
| Game | Your dump | MD5 |
|---|---|---|
| Boss Rally | BIN/CUE image of the retail CD | BIN \`31c64f9b1e09788c2dfc384b44af8f6c\`, CUE \`a48a4a5860558177c3041afee57e03c9\` (another ripper's cue sheet with the same track layout is accepted) |
| Top Gear Rally | ROM of the USA cartridge | \`6f7030284b6bc84a49e07da864526b52\` in .z64 byte order (.v64 and .n64 dumps are accepted) |

### What it makes
- Boss Rally: \`Boss Rally.app\` (macOS) or a \`Boss Rally\` folder with \`Boss Rally.exe\` (Windows), with the CD's files and its soundtrack (FLAC).
- Top Gear Rally: \`Top Gear Rally.app\` (macOS) or a \`Top Gear Rally\` folder with \`Top Gear Rally.exe\` (Windows).

Saves live in your user folder (Application Support on macOS, AppData on Windows), so rebuilding never touches them. Source: [builder/](https://github.com/jeff-strutb/brally/tree/$(git rev-parse HEAD)/builder).
EOF

if [ $DRY = 1 ]; then
    cat $NOTES
    ls -la $MAC $WIN
    exit 0
fi
if gh release view $TAG >/dev/null 2>&1; then
    # the same version again: its tag moves to the source these files are from
    git tag -f $TAG $REV >/dev/null
    git push -q -f origin refs/tags/$TAG
    gh release upload $TAG $MAC $WIN --clobber
    gh release edit $TAG --notes-file $NOTES
else
    gh release create $TAG $MAC $WIN --target "$REV" --title "Rally Builder $VERSION" --notes-file $NOTES
fi
rm -f $NOTES $NOTES.m2
gh release view $TAG --json url -q .url
