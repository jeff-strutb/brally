# N64 remaster renderer

*Recorded 2026-09-29.*

> The approved method for remastering the N64 XM soundtrack with modern instruments (Jungle done; project lead called it nailed) and the mistakes that made earlier passes muddy/dissonant.

2026-09-29: Jungle (xm_17FD10) remastered in stereo and APPROVED by the project lead ("fucking nailed it"). Output and full renderer in `reference/tgrally/XM/remaster/` (renderer/ has code, NAM models, cab IR, README).

Method that worked:
- Log the XM performance with a patched copy of tools/tgrally/xm_render.c (XM_EVENTLOG: per channel per tick note start, playback rate, final volume, pan); render instruments from that, not by swapping samples in the module.
- Tune everything to A440 ET: the ROM samples are detuned up to 46 cents from each other; that was the "dissonance".
- Chord voicings transcribed from each ROM sample (NNLS harmonic fit), moved into playable guitar shapes.
- Guitars: CC0 FSBS clean DI -> NAM capture (own PyTorch inference of .nam v0.5 WaveNet; Helga B 5150 BlockLetter) -> Science Amplification V30 SM57 IR, double-tracked L/R.
- Synths: own C virtual-analog engine (PolyBLEP unison, sub, ZDF ladder), not resynthesis of 8-bit samples.
- Mix: track levels from original stems' K-weighted balance, then modern chain and a master tonal target curve.

Mistakes not to repeat: matching the 8-bit originals' EQ (made it tinny and muddy); stacking separately distorted notes; keeping per-sample detune; setting a sub-heavy synth bass by K-loudness (+8 dB of sub). Piano in 162 (better piano) is torrent-only and had no peers; Salamander used.

All six pieces done 2026-09-29: looping 24-bit FLACs + remastered.json + CREDITS.md in `ports/common/music` (560 MB, not yet committed). Game: music.m has a third source ST_REM faded over the Tab version when Remastered (~, host_fx.m hfx_toggle -> nmusic_remastered) is on; package_app.sh copies it to Resources/music/remastered; verified by recording the output (BR_MUSICWAV) with scripted ~ presses. Generic pipeline: piece_render.py (RECIPES per piece), engines.py (VSCO strings/brass, FreePats SFZ bass, measured DRS percussion, breakbeat rebuild, fitted 909 kick), mixp.py; two-pass event logs for seamless loops with a 120 ms end crossfade. Classification lesson: multi-hit noisy samples are loops (rebuild), steady-pitch "drums" are basses, 80->40 Hz sweeps are 808 kicks.

2026-09-30 v2 of Title/Desert/Mountain/Coastline after project lead feedback (busy, out of rhythm, repetitive, too symphonic). Root causes found by measuring each part against the original's solo stem (onset agreement per 20 s, section loudness, shares):
- Volume per tick must be HELD (step), not linearly interpolated: interpolation starts every gate/slide change a tick early and fills the gaps of gated stabs/pads.
- Balance must be solved AFTER the master tonal EQ (it had pushed bass to ~50% of the mix vs ~20%); cap the cumulative master curve.
- Keep the original's section loudness contour (glue compression + normalisation flattened builds: intro -7 dB became -2 dB).
- Short pitched-up "break" samples: rebuild hits only on the song's real row grid (swing included, from the event log), classified by band rises; the old onset slicer turned kick tails into off-grid kicks.
- Synth amp envelopes fitted to the sample (pads had 350 ms swells where originals hit in 1-10 ms); a bass with sustained tone must not be a plucked sample.
- Chord shapes from NNLS can be one note's harmonic series; wide doubled voicings + 7-voice unison read as orchestral.
- Kick fits must measure the body pitch (Coastline settles at ~77 Hz, fitter grid stopped at 60).

2026-09-30 status: Desert and Coastline APPROVED by the project lead. Mountain rebuilt orchestral (VSCO 2 CE spic/pizz/brass/timpani), Title uses a Sonatina "aah" choir (lib/choir, from Virtual Playing Orchestra) on the top line only, Strip Mine "organic" (guitar power-chord stabs, NOT brass: brass stabs read as harmonica and were rejected). User taste: no literal added impacts (a lull then the band's own downbeat), no synthy "yowp" slides, no constant drones, one lead at a time. Sampled notes must be attack-aligned (DI guitars start 4-49 ms late, spiccato ~50 ms) or riffs sound sloppy.

Splice INSTRUMENT (installed by the project lead; LABS Micah's Choir, Cello Moods): rendered offline with pedalboard hosting the AU (`/Library/Audio/Plug-Ins/Components/Splice INSTRUMENT.component`; the VST3 editor crashes with a use-after-free in its WebView). Patches are chosen once by the project lead in the AU window (scratch splice_pick.py) and saved as plugin state (splice_micah_au.state, splice_cello_au.state); .zpreset/.spitfire files are encrypted and are NOT to be decrypted. Micah's Choir sounds an octave above the played note. 2026-10-04: the /tmp scratch workspace was wiped (Title FLAC lost too). Renderer + libs now live DURABLY at tools/tgrally/music (working data in build/tgrally/music) (git-ignored): README there has the full rebuild (restore.sh, restore_vsco_choir.sh, romsamp -> pieces, xmlog event logs, pick3.sh Splice picks). Code was rebuilt by replaying the transcript (replay_edits.py); two replay mix-ups fixed by hand (romsamp.py from XM/tools, synth.c from remaster/renderer). Coastline re-render matched the shipped FLAC (corr 1.00000). Never keep the remaster workspace in /tmp again.
2026-10-01: Coastline v5 (true chord voicings: C7 pad, doubled top notes) and Mountain v14 (round legato synth bass, no 16th hats, spiccato stabs voiced on E, soft pluck under Cello Moods, 2.6 s hall, humanised NoBoost guitars: down/up strums, drift, eighths) are final. User dislikes rapid machine-identical sharp attacks ("noise"): measure sharp-attacks/s per part vs the original stem and phrase legato. LABS Strings Short/Ensemble do not articulate fast repeats when hosted offline.
2026-09-30 (evening): RULE: every piece uses the Jungle drum kit recipe (engines.jhit: DRSKit with Jungle's mics, velocity-scaled hits, round robin, no synth layers); more kit pieces OK, same quality. Strip Mine final = gritty synths (driven blips/basses/saw stabs, amped trance lead, gritty pad), NOT organic/LABS strings. Mountain opening guitar = palm-muted eighth-note pulse.  A loop mix peaked at 23.7 GB and parallel mixes crashed the Mac: mixer now float32 + 25% interleaved balance sample (13 GB); run renders/mixes strictly one at a time. Check scripts are chmod +x before nohup.
2026-09-30 (later): Strip Mine revised and re-approved ("fucking nailed it"): Lap Steel Plucks (stabs 12/13), Hyperpop Umbriel (arp 6), LABS Strings Ensemble (21), Expressive Strings (14). Splice picker must write state before parsing it; background plugin renders can die silently if a plugin window is still closing -- check track names in the npz before export.
2026-09-30: ALL SIX APPROVED (Title, Desert, Mountain, Coastline, Strip Mine, Jungle); final FLACs + remastered.json + CREDITS.md in ports/common/music. ports/common/ is gitignored by the project lead's decision (stored/packaged outside git, commit 7d68937a). Renderer code (piece_render.py, engines.py, mixp.py, export_loop.py, splice_render.py, splice_*.state patches) saved 2026-10-01 to reference/tgrally/XM/remaster/renderer/v2/; the 12 GB sample library (DRSKit, FSBS guitars, Salamander, VSCO, choir...) was deleted with the scratch, names in v2/LIB_CONTENTS.txt -- re-download to re-render.

Related: [n64-xm-sample-provenance-2026-09-29](n64-xm-sample-provenance-2026-09-29.md).
