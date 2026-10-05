# N64 xm sample provenance

*Recorded 2026-09-29.*

> Where the six N64 XM modules' samples came from, and the 8 higher-quality source samples saved to reference/tgrally/XM/ (gitignored) with manifest.

2026-09-29: traced the N64 soundtrack's ~110 non-empty samples to their origins. The modules are Barry Leitch's; four are byte-identical to Mod Archive tgrtit/tgr3/tgr4/tgr5.xm (tgr1/tgr2 differ slightly). Samples were lifted from 90s scene modules (Counterpoint's f_o_c.mod + dazzler.mod, Brimble's Project-X via anarchy-2, rave_opera.s3m, permian.xm, windscrn.xm, purespin.s3m, Rhino modules, Twisted Edge Snowboarding) and the Amiga ST-XX disks (archive.org AmigaSTXX).

Saved to `reference/tgrally/XM/` (gitignored): 8 donor copies that are the same recording at higher rate / 16-bit / untruncated, plus manifest.csv/json with source URL and evidence (RAVE OPERA bit-exact; 909 corr 1.0 unclipped; rest corr 0.977-0.999). Ratio column = how the ROM was resampled.

Composer-made derivatives (not separately sourceable): 037/049/058.iff = ST-58 rsstring stacked as chords at those semitones; grandpiano3/4 = ST grandpiano pitched +3/+4 st. 12 samples still unsourced (sd1, wack10, "exchange" by gt, uncle ben/gigatron, from the demo, and 7 unnamed).

**The full record is `reference/tgrally/XM/SAMPLE-SOURCES.md`** (every sample, verdict, sources, settled verdicts; regenerate with the scripts in `reference/tgrally/XM/tools/`). Read it before any sample-source work; do not relitigate. Correction logged there: Mod Archive tgr1/tgr2 are ROM rips, never sources (earlier "84 traced" counted them). Count is 96 samples with audio, not 110.

Final state (all searches done 2026-09-29): 87 of 96 traced (51 exact, 20 altered, 7 used upgrades, 1 longer-only, 3 composer chords, 5 probable), 9 not found (sd1, Mountain #0, seven Mountain NoName). Searched: Mod Archive, full Modland (164,749), ST-XX, Sounds Terrific I/II, Da Capo, Aminet smpl/inst/instr, JV-1080 ROM, Trinity installer data, TR-Rack/SC-88/D-550/D-20 notes. No new upgrades: 1,163 higher-rate/16-bit candidates were padded 8-bit, rescaled, or upsampled (distinct-value count is the decisive 16-bit test).

Matching tools lived in the session scratch (modsamp.py MOD/XM/S3M/IT incl. IT214/215 decompression, ncc resample matcher); rebuild if needed. See [xm-soundtrack-oracle](xm-soundtrack-oracle.md), [mac-app-and-music-2026-09-29](../../brally/port/mac-app-and-music-2026-09-29.md).
