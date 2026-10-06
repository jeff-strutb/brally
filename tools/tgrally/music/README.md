# Remastered soundtrack: working renderer

Everything needed to render the six remastered pieces in `ports/common/music`. The scripts live
here; they run in `build/tgrally/music/`, which holds the large, regenerable working data (the
Python venv, the downloaded sample libraries, the amp captures, renders, logs and the compiled
helpers). Each shell script changes into that folder itself and puts this folder on PYTHONPATH,
so the commands below are run from `build/tgrally/music/` with `T=tools/tgrally/music` (path from
the repository root): `venv/bin/python $T/piece_render.py ...`. The third-party amp captures and
cabinet impulse response the restore copies in stay in `reference/tgrally/XM/remaster/renderer/`.
Earlier stages of the renderer are kept under `history/`.

## Rebuild from nothing

1. `./restore.sh` creates `venv/` (numpy, scipy, soundfile, torch, pedalboard, mido, py7zr), copies the amp
   captures from `../renderer/*.nam` into `nam/`, compiles `xmlog` (event logger) and `libsynth.dylib`, and
   downloads into `lib/`: Salamander Grand Piano V3 48k/24, DRSKit 2.1, FreePats Finger Bass YR,
   Picked Bass YR and FSBS clean guitar, and the Science Amplification 4x12 IRs into `ir/science/`.
2. `./restore_vsco_choir.sh` downloads the VSCO 2 CE sections the recipes use into `lib/vsco/`, the
   Sonatina chorus (Virtual Playing Orchestra) into `lib/choir/` and the Mihai Sorohan vowel choir into
   `lib/mihai/`.
3. Modules and event logs:
   `venv/bin/python -c "import romsamp; ..."` writes `pieces/<piece>.xm` from the ROM (offsets 0xebc00
   title, 0x113660 desert, 0x12eab0 mountain, 0x149c80 coastline, 0x164b60 stripmine, 0x17fd10 jungle);
   `XM_EVENTLOG=pieces/<p>.csv ./xmlog --rate 48000 --passes 1 --fade-ms 0 pieces/<p>.xm pieces/<p>.xmlog.wav`
   and the same with `--passes 2` into `loop2/` (the loop versions; `loop2/<p>.json` holds the loop points).
4. Splice INSTRUMENT patches (Spitfire LABS, installed through the Splice app): `pick3.sh` opens the plugin
   window three times; choose Micah's Choir / Sustain Ahhs, Cello Moods / C Awe, Hyperpop Synths / Umbriel.
   The picks are saved as `splice_*_au.state`. The patch files themselves are encrypted and are never
   read directly; the plugin renders them.

## Render one piece (one at a time: a loop mix peaks near 13 GB)

    EVENTS_DIR=loop2 venv/bin/python piece_render.py <piece>
    TRACKS=loop2/%s_tracks.npz NOTRIM=1 ORIGWAV=loop2/%s.xmlog.wav venv/bin/python -c "import mixp; mixp.mix_piece('<piece>','loop2/<piece>_mix.wav')"
    ./exp_one.sh <piece> <Name> <version>        # export FLAC + remastered.json entry, preview MP3

Jungle uses its own approved pipeline (`jungle_render.py`, `mix.py`).

## Source of this code

Rebuilt on 2026-10-04 by replaying every recorded edit (`replay_edits.py`); verified by re-rendering
Coastline, which matched the shipped file to correlation 1.00000 (difference 62 dB below the music).
