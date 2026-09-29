#!/usr/bin/env python3
"""The N64 soundtrack for Boss Rally.app: Top Gear Rally's music modules,
unpacked from the ROM as they are, plus which one the N64 game plays where.

    extract_modules.py "Top Gear Rally (USA).z64" outdir

writes outdir/xm_XXXXXX.xm (XXXXXX = the module's ROM offset, the naming
tools/extract_xm.py uses) and outdir/modules.json:

    {"title": "xm_0EBC00.xm", "race": ["xm_113660.xm", ...5 entries]}

Both cues are the N64 game's own, read from its code and data, not chosen:
  title  BrMainMenu (n64/src/menus/mainmenu.c) unpacks ROM 0x0EBC00 and
         starts it on the front end.
  race   BrMusicLoadTrack (n64/src/startup/main.c) unpacks
         D_8026FF24[track]; that table sits at ROM 0x70F24 and holds one
         module per track, 0-4, repeated for the mirrored tracks 5-9.
The port plays the modules live (native/music.m), so nothing is rendered.
"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'tools'))
import extract_xm as xm   # noqa: E402

TITLE = 0x0EBC00
RACE_TABLE = 0x70F24      # D_8026FF24
RACE_TRACKS = 5


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
    os.makedirs(out, exist_ok=True)
    for off, payload in mods.items():
        with open(os.path.join(out, 'xm_%06X.xm' % off), 'wb') as f:
            f.write(payload)
    with open(os.path.join(out, 'modules.json'), 'w') as f:
        json.dump({'title': 'xm_%06X.xm' % TITLE,
                   'race': ['xm_%06X.xm' % r for r in race[:RACE_TRACKS]]}, f, indent=1)
        f.write('\n')
    print('modules: %d unpacked -> %s' % (len(mods), out))


if __name__ == '__main__':
    main()
