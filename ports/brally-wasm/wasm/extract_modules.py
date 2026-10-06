#!/usr/bin/env python3
"""The N64 soundtrack for Boss Rally.app: Top Gear Rally's music modules,
unpacked from the ROM as they are, plus which one the N64 game plays where.

    extract_modules.py "Top Gear Rally (USA).z64" outdir

writes outdir/xm_XXXXXX.xm (XXXXXX = the module's ROM offset, the naming
tools/tgrally/extract_xm.py uses) and outdir/modules.json:

    {"title": "xm_0EBC00.xm", "race": ["xm_113660.xm", ...5 entries],
     "race_names": ["Desert", ...]}

Both cues are the N64 game's own, read from its code and data, not chosen:
  title  BrMainMenu (src/tgrally/menus/mainmenu.c) unpacks ROM 0x0EBC00 and
         starts it on the front end.
  race   BrMusicLoadTrack (src/tgrally/startup/main.c) unpacks
         D_8026FF24[track]; that table sits at ROM 0x70F24 and holds one
         module per track, 0-4, repeated for the mirrored tracks 5-9.
         race_names are those tracks' names, from the track records at
         ROM 0x71854 (0x80270854).
The port plays the modules live (native/music.m), so nothing is rendered.
"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'tools', 'tgrally'))
import extract_xm as xm   # noqa: E402

TITLE = 0x0EBC00
RACE_TABLE = 0x70F24      # D_8026FF24
RACE_TRACKS = 5
TRACKS = 0x71854          # the track records, 0x80270854, stride 0x17C
TRACK_STRIDE = 0x17C
RAM_TO_ROM = 0x801FF000   # this segment's load address minus its ROM offset


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    rom_path, out = sys.argv[1], sys.argv[2]
    rom = xm.normalise_rom(open(rom_path, 'rb').read(), rom_path)
    mods = dict(xm.find_modules(rom))
    if TITLE not in mods:
        sys.exit('extract_modules: no module at the title offset 0x%06X' % TITLE)
    race = [xm.be32(rom, RACE_TABLE + 4 * i) for i in range(2 * RACE_TRACKS)]
    if any(r not in mods for r in race) or race[:RACE_TRACKS] != race[RACE_TRACKS:]:
        sys.exit('extract_modules: ROM 0x%X is not the per-track music table: %s'
                 % (RACE_TABLE, ' '.join('%06X' % r for r in race)))
    # each track record starts with its menu item, whose first word points
    # at the track's name
    names = []
    for i in range(RACE_TRACKS):
        p = xm.be32(rom, TRACKS + i * TRACK_STRIDE) - RAM_TO_ROM
        names.append(rom[p:p + 32].split(b'\0')[0].decode('latin-1'))
    os.makedirs(out, exist_ok=True)
    for off, payload in mods.items():
        with open(os.path.join(out, 'xm_%06X.xm' % off), 'wb') as f:
            f.write(payload)
    with open(os.path.join(out, 'modules.json'), 'w') as f:
        json.dump({'title': 'xm_%06X.xm' % TITLE,
                   'race': ['xm_%06X.xm' % r for r in race[:RACE_TRACKS]],
                   'race_names': names}, f, indent=1)
        f.write('\n')
    print('modules: %d unpacked -> %s' % (len(mods), out))


if __name__ == '__main__':
    main()
