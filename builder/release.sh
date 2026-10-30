#!/bin/sh
# builder/release.sh: build Rally Builder (it builds Boss Rally, Top Gear
# Rally or Top Gear Rally Remastered) for macOS and Windows and publish both on
# the repository's GitHub Releases page as vVERSION.
#
# VERSION is version.py's: the average of the two decompilations' M2
# progress, or its RELEASE while that is ahead of M2.
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
Rally Builder makes a native build of a game from your own copy of it. Pick one of three:
- **Boss Rally** (PC, 1999), from your retail CD, with its CD soundtrack.
- **Top Gear Rally** (Nintendo 64, 1997), from your cartridge ROM: the USA's, Europe's or Japan's. The build is that cartridge's game.
- **Top Gear Rally Remastered**, from the USA and Japan ROMs and the CD: Top Gear Rally's menus, seasons and soundtrack, with every race run by Boss Rally's engine on the same tracks.

On a Mac, Boss Rally and Top Gear Rally build for the Mac, and Remastered for the Mac or for the iPhone and iPad; on Windows, each builds for Windows. The games' data is copyrighted, so the builder does not include it: you provide your dumps, the builder checks them by MD5 (a dump that does not match can still be built after a warning), takes what the game needs from them, and produces a game that runs on its own. The dumps are not needed once the build is done.

The games are the decompilations' own code, compiled natively: Top Gear Rally's is byte-exact to each of its three cartridges, every function, and Boss Rally's behaves exactly as the retail game does, every function, most of them byte-exact too. The version follows that byte-exact (M2) progress, the average over the three cartridges and Boss Rally:
\`\`\`
$(cat $NOTES.m2)
\`\`\`

### In this release
- **Build Anyway**: a dump whose MD5 does not match (another release of the game, a modified copy, a different rip) no longer stops you. Choose it and press Build: the builder lists the files that do not match and warns that the build may fail or the game may not work properly, then builds if you choose Build Anyway. A ROM is read as the cartridge its header names. If the build fails, an error screen says why, names the files that did not match, and nothing is left behind in the folder.
- **Top Gear Rally**: all three cartridges. The USA's, Europe's (at its own 25 frames a second, PAL 50 Hz, with its Game Pak saves) and Japan's (its own title, Japanese text and real liveries, its three cars): give the builder any of the three ROMs.
- **Remastered, menus**: Menus chooses Top Gear Rally (USA), Top Gear Rally (PAL/Japan) (Japan's own menus, title and liveries, in English, with the cartridge's Game Pak saves) or Boss Rally's own. Music chooses Boss Rally's CD, Top Gear Rally (USA) or Top Gear Rally (PAL/Japan) (the two cartridges' front-end tunes).
- **Remastered, cars and cheats**: Japan's Type-PJ, Type-TR and Type-MN beside the Type-RS, Type-M3 and Helmet they replaced, each unlocked with its twin. Europe's and Japan's cheats, on your own controls: on the main menu hold Gear Up and Gear Down and press Change View three times for every car, course and weather; in a race, Gear Up, Gear Up, Steer Right, Accelerate, Change View, Steer Right finishes it in first place.
- **Remastered, controls**: one set of controls for the menus and the races, every action one N64 button, with keyboard defaults where no key does two things (X select and accelerate, Z back and brake, the arrows steer and move, W and Q change gear, Escape pauses); rebind anything on the controller screen. Every prompt on screen names your own key or pad button, in the cartridge's colours; on the iPhone and iPad they say tap and swipe.
- **Remastered, rumble**: a game controller shakes as Top Gear Rally's Rumble Pak did, in every race (XInput on Windows, Core Haptics on the Mac, the iPhone and the iPad).
- **Remastered, paint shop**: works by mouse and by touch: the cursor follows the pointer, a click or a finger paints, drags out lines and shapes, picks tools and colours and presses a popup's buttons; the right button (a two-finger tap) is B.
- **Remastered, everything else**: no loading screen before a race (they load in under a tenth of a second); widescreen menus that slide and wipe to the window's edges; on the iPhone and iPad no Quit, no two-player screen and no network play. Fixed: a championship round's end no longer crashes, and coming back to Top Gear Rally's menus from the other menus no longer drops into the attract demo.

### Download
- **macOS** (12 or later, Apple silicon and Intel; the games it builds need the same): \`RallyBuilder-$VERSION-macOS.zip\`. Unzip and open Rally Builder. It is not notarized: if macOS refuses to open it, Control-click it and choose Open, or allow it in System Settings, Privacy & Security.
- **Windows** (10 or later, 64-bit; the games it builds need the same, and a Vulkan driver or OpenGL 3.3): \`RallyBuilder-$VERSION-Windows.zip\`. Unzip it and run \`RallyBuilder.exe\` from the unzipped folder (the \`games\` folder beside it holds the games' code). It is not signed: if SmartScreen stops it, choose More info, then Run anyway.
- Linux is not built yet.

### What it needs
| Game | Your dump | MD5 |
|---|---|---|
| Top Gear Rally (for Top Gear Rally: any one; for Remastered: the USA's and Japan's) | ROM of the cartridge | USA \`6f7030284b6bc84a49e07da864526b52\`, Europe \`1698508f521280d0a80e078ec981d4ac\`, Japan \`6e0af13dcefee6a11c4d7262206d6d2d\`, in .z64 byte order (.v64 and .n64 dumps are accepted) |
| Boss Rally (for Boss Rally and Remastered) | BIN/CUE image of the retail CD | BIN \`31c64f9b1e09788c2dfc384b44af8f6c\`, CUE \`a48a4a5860558177c3041afee57e03c9\` (another ripper's cue sheet with the same track layout is accepted) |

### What it makes
- Mac: \`Boss Rally.app\`, \`Top Gear Rally.app\` (Europe's and Japan's: \`Top Gear Rally (Europe).app\`, \`Top Gear Rally (Japan).app\`) or \`Top Gear Rally Remastered.app\`.
- iPhone and iPad (Remastered, from the Mac builder, iOS 16 or later): \`Top Gear Rally Remastered.ipa\`. When your Mac has Xcode signing (a development certificate with a provisioning profile for it), the .ipa is signed with it, ready to install from Xcode (Devices and Simulators) or Apple Configurator. Otherwise the builder says so in red and the .ipa is unsigned, for you to sign and install with a tool such as AltStore or Sideloadly.
- Windows: a folder named for the game, with the game's .exe in it.

### Playing
- Full screen: View, Enter Full Screen (Control-Command-F) on a Mac, Alt+Enter on Windows.
- **Boss Rally** is the PC game as it shipped, its menus and its keys; its pause menu has Quit Game.
- **Top Gear Rally** is the cartridge your ROM is from: the USA's and Japan's at their own 30 frames a second, Europe's at its 25 (PAL). Saves are a Controller Pak kept in your user folder, and Europe's and Japan's Game Pak (EEPROM) beside it. A gamepad or the keyboard is the N64 pad (X or Return is A, Z is B, the arrows the stick, Escape START); Options, Keys and Pad changes that. Escape on the main menu, or the pause menu's Exit Game, quits.

### Top Gear Rally Remastered
The menus are Top Gear Rally's; each race is run by Boss Rally's engine at 60 ticks a second and its result goes back into Top Gear Rally's results, season and records. The options screen adds **Widescreen** (the races and menus fill any window shape), **Music** (Boss Rally's CD, or Top Gear Rally's soundtrack with the USA's or the PAL/Japan front-end tune), **Show FPS**, **View Distance** (Unlimited: nothing pops in) and **Menus** (Top Gear Rally (USA), Top Gear Rally (PAL/Japan), or Boss Rally's own front end with its championship, time attack and quick race). Two players race split screen on a desktop, side by side when Widescreen is on. The controller screen sets each player's keys and gamepad buttons, for the menus and the races alike; Change View steps through the car's view, the chase view and the bonnet. A game controller rumbles as the Rumble Pak did. The paint shop works by mouse and touch. Quit is on the main menu (not on the iPhone and iPad). On the iPhone and iPad, touch drives it: in a race a finger on the right steers and accelerates, one on the left brakes; in the menus, tap and swipe; two fingers pause.

Saves live in your user folder (Application Support on macOS, the iPhone and the iPad, AppData on Windows), so rebuilding never touches them. Source: [builder/](https://github.com/jeff-strutb/brally/tree/$(git rev-parse HEAD)/builder), [ports/](https://github.com/jeff-strutb/brally/tree/$(git rev-parse HEAD)/ports).
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
