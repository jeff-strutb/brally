# Top Gear Rally (N64) soundtrack: where every sample came from

This records the source of every sample in the six music modules of the N64 ROM, how each match was proved, and which sources are better than the ROM copy. It exists so none of this has to be worked out again.

Last updated 2026-09-29. All searches are complete (Modland 164,749 of 164,749 modules).

## Summary

The six modules hold 96 samples with audio (empty slots and 2-byte placeholders are not counted).

| Verdict | Samples |
|---|---|
| exact source | 51 |
| same recording, altered | 20 |
| better copy, used | 7 |
| longer copy, not used | 1 |
| composer-made | 3 |
| probable only | 5 |
| NOT FOUND | 9 |

- **exact source**: a byte-identical copy of the ROM sample exists in an older module or sample disk (listed). Same 8-bit quality, so nothing to gain.
- **same recording, altered**: the same recording exists, but the ROM copy was resampled, pitched, amplified, trimmed or edited, so it is not byte-identical. Correlation 0.97 or better over the ROM sample; a correlation of 1.000 at the same rate means only the volume differs. Unless marked as used, the source is no better than the ROM copy.
- **better copy, used**: the source is 16-bit, a higher rate, or both. These seven WAVs are in this folder and `package_app.sh --hq-samples` swaps them in (see the repo README, Mac port).
- **composer-made**: built by the composer from another sample (chords), so only the ingredient can be sourced.
- **probable only**: a likely source correlating below 0.97. Not treated as proven.

## Settled verdicts (do not relitigate)

- **The modules are Barry Leitch's.** Mod Archive `tgrtit.xm`, `tgr3.xm`, `tgr4.xm` and `tgr5.xm` (and Modland `Fasttracker 2/Barry Leitch/tgr*.xm`) are byte-identical to four of the ROM modules; `tgr1.xm` and `tgr2.xm` differ slightly. These are rips of this game, so they are never counted as sources. `tgrjtit.xm` is a 14-channel variant of the title.
- **Most samples are borrowed.** They come from late 80s and 90s Amiga and PC tracker modules and the Amiga ST-XX sample disks. The sample names are often text lines from the donor module (musicians wrote messages in the sample-name slots), which is how many donors were found: "for more MODs in the / near future!!!!" is Counterpoint's f_o_c.mod, "this piece is 100% of / my mind... / all samples by me" is dazzler.mod, "also brimble for" is Allister Brimble's Project-X, and so on.
- **Almost nothing better exists.** The donors are themselves 8-bit Amiga-era material. Only the seven samples marked "better copy, used" have a better copy anywhere searched.
- **XXBASS.WAV in x4songa.it is not the xxbass sample**, despite the name (correlation 0.20).
- **The three .iff chords (037, 049, 058) are chords of ST-58 rsstring**, named after their semitone offsets.
- **grandpiano3/4 are ST-86 grandpiano3/4 pitched up 3 and 4 semitones**, then amplified.

## Where was searched

- The Mod Archive (modarchive.org): instrument-text search on every sample name, Barry Leitch's artist page (40 modules), all ten Twisted Edge Snowboarding modules, and about 110 donor candidates, downloaded and compared.
- The Amiga ST-XX sample disks, 10,555 samples (archive.org item AmigaSTXX, ST-XX.zip).
- Modland (ftp.modland.com): every Barry Leitch module in a sample-based format (216 files), then a full scan of its Protracker, Soundtracker, Screamtracker 3, Multitracker, Composer 669, Ultratracker, Fasttracker 2 and Impulsetracker folders (164,749 modules, about 74 GB, streamed and not kept).

## Also checked, no match

- **Roland JV-1080 wave ROMs** (all 8 MB, decoded: 16-bit word scrambling per the emuscd project, then Roland FCE delta decoding). Best score of any game sample 0.87 against a chance level of 0.57. Barry Leitch has said the JV-1080, Kurzweil K2500 and Korg Trinity were used for the synth track of the intro video, not the modules.
- **Korg Trinity sample data** (the four KSCSNDRAW banks and 32 MB of 8-bit PCM in the TRINITY v1.1.4 installer). Best 0.927, the 909 kick, which is a similar 909 kick and not the recording (its real source, permian.xm, matches 1.00); chance level 0.60.
- **Korg TR-Rack notes** from freewavesamples.com (86 files, rendered through presets): best 0.91 against chance 0.52.
- **Roland SC-88 Slap Bass, D-550 and D-20 notes** from freewavesamples.com: no match to Slapbas2 or @mellow-d (0.58 and 0.60). There are no Juno-106, Alpha Juno or D-50 samples on that site.
- **Amiga sample CDs**: Sounds Terrific I (1994) and II (1996) by Weird Science, Da Capo Vol. 1, and every archive in Aminet mods/smpl, mods/inst and mods/instr (76,590 files). They repeat known sources and add no new ones.

## Candidate upgrades rejected

The full scan flagged 1,163 copies at a higher rate or in 16-bit that match a game sample at 0.97 or better. None beat the seven in use. The tests that rejected them:

- **16-bit files holding 8-bit data.** A genuine 16-bit sample has thousands of distinct values (the 909 in use: 6,057). Copies in Pro-XeX, DJ AIL, DJ Keen, Patosz, Draygen, Matley and similar modules have 1 to about 300: 8-bit samples rescaled or padded into 16-bit. DJ AIL's copy of "too much homework" is the game's own bytes scaled (residual -167 dB).
- **Upsampled copies.** Badliz's 16-bit copies at twice the rate have every other sample exactly on an 8-bit step. The 3.5x "near future!!!!" in Phoenix's nameless.it has only interpolation-level content above the original band (-22 dB), and the game's copy is byte-identical to Counterpoint's 1993 original, so it is an upsample of that.
- **Same data, no gain.** Many higher-rate 8-bit copies (Michael K. Berg in Lord Mystic's modules, the pianos in Falcon's death-box and Sounds Terrific's PIANO5.SND) are the same as the copy already in use.

The seven in use pass all of these. The 909's gain is bit depth only: its doubled rate adds nothing above the original band.

Modland files the sample reader could not parse (81, mostly unusual IT and XM variants) were not checked.

## How a match is proved

1. **Exact**: the ROM sample's bytes occur in the source sample (16-bit sources compared by their top 8 bits).
2. **Same recording at another rate or pitch**: the source is resampled by the length ratio (or a semitone grid) and cross-correlated with the ROM sample, normalised. 0.97 or better over the whole ROM sample is treated as the same recording. The bulk scan pre-filters with a 48-band loudness envelope and the zero-crossing count (both unchanged by resampling) before correlating.
3. **Bit-exact derivation** where possible: search simple converters (nearest, linear, averaging; gain; rounding) for one that rebuilds the ROM bytes. Only the Rave Opera string rebuilds exactly; the rest were resampled with filtering or amplified into clipping, which cannot be undone.

The scripts are in `tools/` beside this file (not part of the repo): `modsamp.py` reads MOD, XM, S3M and IT samples (including IT 2.14/2.15 compressed samples), `match2.py` and `verify.py` do the correlation and converter search, `scan.py` is the streaming Modland scanner, `consolidate.py` and `writedoc.py` build this file. `tools/data/` keeps every scan's results, so the file can be rebuilt without searching again: run the two scripts from `tools/data/` with `tools/` on the Python path.

## Every sample

### Title piece (xm_0EBC00)

| # | Sample name | Length | Verdict | Sources |
|---|---|---|---|---|
| 0 | started late '96 | 35012 | same recording, altered | Mod Archive #165338 rhino_-_rose_garden.xm (same rate, 8-bit, correlation 1.000); Mod Archive #152227 yestrday.xm (same rate, 8-bit, correlation 1.000); Mod Archive #901 sick.xm (same rate, 8-bit, correlation 1.000); Modland: Impulsetracker/X-Rabbit/elysium eternal.it (same rate, 8-bit, correlation 1.000); and 267 more |
| 1 | NoName | 3539 | better copy, used | UPGRADE, in use. windscrn.xm (Mod Archive #63094) instrument 8, sqrt(2) times the rate. Correlation 0.994; 92% of the ROM bytes are identical to plain decimation of it. Other copies: Modland: Fasttracker 2/Shifter/unreal dreamz.xm (exact (scan)); Modland: Fasttracker 2/- unknown/boundless universe.xm (exact (scan)); Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); Modland: Impulsetracker/DNA-Groove/perky princess.it (exact (scan)); and 161 more |
| 2 | also brimble for | 7068 | same recording, altered | Mod Archive #155390 anarchy-2-1629.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Vince (DE)/brainwalk.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/- unknown/ing6.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Barry Leitch/anarchy.mod (same rate, 8-bit, correlation 1.000) |
| 3 | K3.WAV | 3452 | exact source | Mod Archive #63094 windscrn.xm (exact); Modland: Fasttracker 2/Arcain/shackle.xm (same rate, 8-bit, correlation 1.000); Modland: Impulsetracker/Autolycus/fifth element (edit).it (same rate, 8-bit, correlation 1.000); Modland: Impulsetracker/Autolycus/the fifth element.it (same rate, 8-bit, correlation 1.000); and 1 more |
| 4 | revcym | 36884 | same recording, altered | Mod Archive #146448 basic_theme_no1_rmx.xm (same rate, 8-bit, correlation 1.000); Modland: Protracker/Alfons/alone in despair.mod (same rate, 8-bit, correlation 1.000); Modland: Screamtracker 3/Digistorm/the regeneration center i.s3m (same rate, 8-bit, correlation 1.000); Modland: Screamtracker 3/Harry/maximum frequency.s3m (same rate, 8-bit, correlation 1.000); and 531 more |
| 5 | 909-pack | 4138 | better copy, used | UPGRADE, in use. permian.xm (Mod Archive #54465) holds it 16-bit at twice the rate. The ROM copy is that halved in rate, cut to 8-bit and amplified until its peaks clip; excluding the clipped peaks the two correlate 1.0000. Other copies: Modland: Fasttracker 2/Rize404/indian summerhippie.xm (exact (scan)); Modland: Fasttracker 2/Rolex/fin sang.xm (exact (scan)); Modland: Fasttracker 2/- unknown/boundless universe.xm (exact (scan)); Modland: Fasttracker 2/Viraxor/a day with friends.xm (exact (scan)); and 246 more |
| 6 | near future!!!! | 8296 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 149 more |
| 7 | (unnamed) | 8336 | exact source | Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); Modland: Fasttracker 2/DJ Schnee/frozen mind.xm (exact (scan)); Modland: Fasttracker 2/OsO/dusty sky.xm (exact (scan)); Modland: Fasttracker 2/- unknown/crimson.xm (exact (scan)); and 476 more |
| 8 | 909 opened hihat | 9380 | exact source | Mod Archive #41500 dtn-toxc.xm (exact); Modland: Fasttracker 2/Alvak/2 hard 2 think.xm (same rate, 8-bit, correlation 1.000); Modland: Fasttracker 2/Batjo/all to-get-her now.xm (same rate, 8-bit, correlation 1.000); Modland: Fasttracker 2/Cactus/mongoose culture.xm (same rate, 8-bit, correlation 1.000); and 183 more |
| 9 | for more MODs in the | 23028 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 281 more |
| 10 | **    The RAVE OPERA | 21360 | better copy, used | UPGRADE, in use. rave_opera.s3m (Mod Archive #70077) sample 4. BIT-EXACT: taking every sqrt(2)-th sample of the source (nearest neighbour) reproduces the ROM copy byte for byte. Other copies: Modland: Screamtracker 3/Mike Genato/bolz-menu 1.s3m (1.414x the rate, 8-bit, correlation 0.942); Modland: Screamtracker 3/Beaner/halloween.s3m (1.414x the rate, 8-bit, correlation 0.939); Modland: Screamtracker 3/Big Jim/forever.s3m (1.414x the rate, 8-bit, correlation 0.939); Modland: Screamtracker 3/Blackwolf/reset button.s3m (1.414x the rate, 8-bit, correlation 0.939); and 608 more |
| 11 | sd1 | 17057 | NOT FOUND | NOT FOUND. Rhino's modules (aura, craft2, rose_garden and others on Mod Archive) use the same instrument text "/4518" and a sample named "sd1", but the audio does not match (best 0.85 at x1.414). |
| 12 | (unnamed) | 18780 | probable only | PROBABLE only. "teknicida" by Atlasz (Modland FT2), instrument 16, at 1.78x the rate (correlation 0.91). Below the bar. Other copies: Modland: Fasttracker 2/Atlasz/teknicida.xm (1.782x the rate, 8-bit, correlation 0.909) |
| 13 | Michael K. Berg | 18283 | better copy, used | UPGRADE, in use. purespin.s3m (Mod Archive #57172) sample 13, twice the rate (correlation 0.98). The same recording, also at twice the rate, is in "do you really" by Mittag-Leffler (Modland, 0.991). Other copies: Modland: Screamtracker 3/Lord Mystic/spirit of the time.s3m (2.000x the rate, 8-bit, correlation 0.993); Modland: Screamtracker 3/Lord Mystic/stars.s3m (2.000x the rate, 8-bit, correlation 0.993); Modland: Protracker/Mittag-Leffler/do you really.mod (2.000x the rate, 8-bit, correlation 0.991); Modland: Protracker/Mittag-Leffler/3 minutes of a life.mod (2.000x the rate, 8-bit, correlation 0.991); and 457 more |
| 14 | (unnamed) | 16790 | same recording, altered | Modland: Fasttracker 2/Jason/zwieback.xm (same rate, 8-bit, correlation 1.000); Modland: Fasttracker 2/Kenet/obsess.xm (same rate, 8-bit, correlation 1.000); Modland: Impulsetracker/Pauli Merilainen/boardwalk.it (same rate, 8-bit, correlation 1.000); Modland: Protracker/Bracus/sad saturday.mod (same rate, 8-bit, correlation 1.000); and 15 more |
| 15 | xxbass | 11292 | exact source | Byte-identical to "xbass" on the ST-A5 disk and to the sample in Allister Brimble's Project-X tune (anarchy-2-1629.mod). NOT the same as XXBASS.WAV in x4songa.it (Mod Archive #155594): that one is 16-bit 44.1 kHz but a different recording (correlation 0.20 at every rate). Other copies: 135113.bin (exact); 135549.bin (exact); Mod Archive #155390 anarchy-2-1629.mod (exact); Modland: Fasttracker_2_Barry_Leitch_twisted_edge_-_snowboard_06.xm (exact); and 13 more |

### Desert piece (xm_113660)

| # | Sample name | Length | Verdict | Sources |
|---|---|---|---|---|
| 0 | Brk9.iff | 12654 | same recording, altered | Modland: Screamtracker 3/Liam The Lemming/misadventurous - techno mix.s3m (same rate, 8-bit, correlation 0.991); Modland: Protracker/Stax/the dawn.mod (same rate, 8-bit, correlation 0.991); Modland: Protracker/DJ Oterola/chase me,find me!.mod (same rate, 8-bit, correlation 0.991); Modland: Protracker/Pinocchio/legs.mod (same rate, 8-bit, correlation 0.991); and 145 more |
| 1 | @cs.uregina.ca | 13994 | exact source | Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); Modland: Impulsetracker/Panther/above the clouds.it (same rate, 8-bit, correlation 1.000); Modland: Impulsetracker/Powermike/heavy sleep.it (same rate, 8-bit, correlation 1.000); and 2 more |
| 2 | (unnamed) | 3236 | same recording, altered | Modland: Protracker/Bit Arts/brass-connection.mod (same rate, 8-bit, correlation 1.000); Amiga disc: st1b/MODULES/B/BRASSCON.MOD (same rate, 8-bit, correlation 1.000); Amiga disc: st2b/MODS/B/BRASSCON.MOD (same rate, 8-bit, correlation 1.000) |
| 3 | (unnamed) | 3120 | same recording, altered | Modland: Protracker/Bit Arts/brass-connection.mod (same rate, 8-bit, correlation 1.000); Amiga disc: st1b/MODULES/B/BRASSCON.MOD (same rate, 8-bit, correlation 1.000); Amiga disc: st2b/MODS/B/BRASSCON.MOD (same rate, 8-bit, correlation 1.000); Modland: Fasttracker 2/Stormlord/coop-Synthetic Rebirth/let's do it.xm (same rate, 8-bit, correlation 1.000) |
| 4 | Brk7.iff | 12936 | same recording, altered | Modland: Screamtracker 3/Liam The Lemming/misadventurous - techno mix.s3m (same rate, 8-bit, correlation 0.996); Modland: Protracker/Stax/the dawn.mod (same rate, 8-bit, correlation 0.996); Modland: Protracker/DJ Oterola/chase me,find me!.mod (same rate, 8-bit, correlation 0.996); Modland: Protracker/Bloody/legoukon painajainen.mod (same rate, 8-bit, correlation 0.996); and 21 more |
| 5 | @mellow-d | 18971 | same recording, altered | Mellow-D's own FT2 module "nouveau monde" (Modland), instrument 1, same length, correlation 0.990. "@mellow-d" is the musician's credit. Not a Juno/D-50 sound (D-550 and D-20 notes tested: best 0.60). Other copies: Modland: Impulsetracker/Coda/donut counting.it (same rate, 8-bit, correlation 0.990); Modland: Fasttracker 2/Mellow-D/nouveau monde.xm (same rate, 8-bit, correlation 0.990); Modland: Fasttracker 2/Mellow-D/nouveau monde (no swing).xm (same rate, 8-bit, correlation 0.990) |
| 6 | Slapbas2.iff | 3348 | same recording, altered | Same recording as the common Protracker sample in "road to the city" (MMB) and "legoukon painajainen" (Bloody), and "STYLEBLE.MOD" on Sounds Terrific: correlation 0.971 over the full length. The composer moved the loop (1733+857 instead of 3058+288) and edited about 400 of its 3348 bytes. Not the Roland SC-55/SC-88 "Slap Bass 2" (SC-88 note tested: 0.58). |
| 7 | near future!!!! | 8296 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 188 more |
| 8 | for more MODs in the | 23028 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 281 more |
| 9 | (unnamed) | 8336 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 492 more |
| 10 | *** or *** | 35138 | exact source | Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); Modland: Protracker/Counterpoint/foc.mod (same rate, 8-bit, correlation 1.000); Amiga disc: st2a/MODS/F/FREEDOMO.MOD (same rate, 8-bit, correlation 1.000) |
| 11 | (unnamed) | 1010 | same recording, altered | Modland: Protracker/Echo/joes#04.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Electro Maniac/nokturnal pain.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Mike Richmond/aquakon title.mod (same rate, 8-bit, correlation 1.000); Modland: Screamtracker 3/Steve Corbett/it's inevitable.s3m (same rate, 8-bit, correlation 1.000); and 153 more |
| 12 | (unnamed) | 15640 | same recording, altered | Modland: Protracker/Prodigy/sunday.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Prodigy/seduction.mod (same rate, 8-bit, correlation 1.000) |
| 13 | (unnamed) | 2024 | exact source | Amiga ST-XX disks: ST-74/dx-xstr001.min (exact); Modland: Protracker/Bit Arts/brass-connection.mod (same rate, 8-bit, correlation 1.000); Amiga disc: st2b/MODS/B/BRASSCON.MOD (same rate, 8-bit, correlation 1.000); Modland: Protracker/Excalibur/falling down.mod (same rate, 8-bit, correlation 0.999) |
| 14 | (unnamed) | 1894 | exact source | Amiga ST-XX disks: ST-74/dx-xstr001.maj (exact); Modland: Protracker/Bit Arts/brass-connection.mod (same rate, 8-bit, correlation 1.000); Amiga disc: st1b/MODULES/B/BRASSCON.MOD (same rate, 8-bit, correlation 1.000); Amiga disc: st2b/MODS/B/BRASSCON.MOD (same rate, 8-bit, correlation 1.000); and 2 more |

### Mountain piece (xm_12EAB0)

| # | Sample name | Length | Verdict | Sources |
|---|---|---|---|---|
| 0 | (unnamed) | 33502 | NOT FOUND | NOT FOUND anywhere searched. |
| 1 | (unnamed) | 3440 | exact source | Modland: Fasttracker 2/Shifter/unreal dreamz.xm (exact (scan)); Modland: Fasttracker 2/- unknown/boundless universe.xm (exact (scan)); Modland: Fasttracker 2/Viraxor/a day with friends.xm (exact (scan)); Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); and 76 more |
| 2 | (unnamed) | 4508 | same recording, altered | Modland: Fasttracker 2/Sandman/at crack of dawn.xm (same rate, 8-bit, correlation 0.996); Modland: Impulsetracker/Arcturus/subliminal.it (same rate, 8-bit, correlation 0.996); Modland: Fasttracker 2/Elwood/into the shadow.xm (same rate, 8-bit, correlation 0.996); Modland: Impulsetracker/Deflex/saro in motion groove.it (same rate, 8-bit, correlation 0.996); and 35 more |
| 3 | Of DoMtOwN rAvEs | 6526 | exact source | 135355.bin (exact); Modland: Fasttracker_2_Barry_Leitch_twisted_edge_-_snowboard_07.xm (exact); Modland: Fasttracker 2/Anthrium/gwb-criminal.xm (exact (scan)); Modland: Fasttracker 2/Chris Meland/coop-Speed Devil/if someone had known.xm (exact (scan)); and 1309 more |
| 4 | for more MODs in the | 12923 | probable only | PROBABLE only. Counterpoint's f_o_c.mod sample 15 ("for more MODs in the", 23028 samples) at 1.782x the rate, i.e. pitched 10 semitones (correlation 0.89). The name agrees; the audio match is below the bar. Other copies: Modland: Fasttracker 2/Zoda/we're breathing again.xm (1.782x the rate, 8-bit, correlation 0.936); Modland: Fasttracker 2/Aureate/dreamcatcher.xm (1.782x the rate, 8-bit, correlation 0.936); Modland: Fasttracker 2/Aureate/elastic ponderings.xm (1.782x the rate, 8-bit, correlation 0.936); Modland: Fasttracker 2/Aureate/forbidden.xm (1.782x the rate, 8-bit, correlation 0.935) |
| 5 | near future!!!! | 5536 | exact source | Modland: Fasttracker 2/Shifter/unreal dreamz.xm (exact (scan)); Modland: Fasttracker 2/- unknown/boundless universe.xm (exact (scan)); Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); Modland: Impulsetracker/DNA-Groove/perky princess.it (exact (scan)); and 189 more |
| 6 | (unnamed) | 4678 | exact source | Modland: Fasttracker 2/DNA Trance/exp into the unknown.xm (exact (scan)); Modland: Fasttracker 2/Shifter/unreal dreamz.xm (exact (scan)); Modland: Fasttracker 2/- unknown/boundless universe.xm (exact (scan)); Modland: Fasttracker 2/Viraxor/a day with friends.xm (exact (scan)); and 671 more |
| 7 | (unnamed) | 1010 | same recording, altered | Modland: Protracker/Echo/joes#04.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Electro Maniac/nokturnal pain.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Mike Richmond/aquakon title.mod (same rate, 8-bit, correlation 1.000); Modland: Screamtracker 3/Steve Corbett/it's inevitable.s3m (same rate, 8-bit, correlation 1.000); and 153 more |
| 8 | NoName | 7301 | NOT FOUND | NOT FOUND. One of seven similar "NoName" samples (7301-7479 samples each) in this piece; probably one sound the composer rendered at several pitches or with different processing. |
| 9 | NoName | 7353 | NOT FOUND | NOT FOUND. One of the seven "NoName" samples, see #8. |
| 10 | NoName | 7406 | NOT FOUND | NOT FOUND. One of the seven "NoName" samples, see #8. |
| 11 | NoName | 7310 | NOT FOUND | NOT FOUND. One of the seven "NoName" samples, see #8. |
| 12 | NoName | 7430 | NOT FOUND | NOT FOUND. One of the seven "NoName" samples, see #8. |
| 13 | NoName | 7382 | NOT FOUND | NOT FOUND. One of the seven "NoName" samples, see #8. |
| 14 | NoName | 7479 | NOT FOUND | NOT FOUND. One of the seven "NoName" samples, see #8. |
| 15 | pure disco pleasure | 6150 | probable only | Mod Archive #36504 dazzler2.mod (1.335x the rate, 8-bit, correlation 0.954); Mod Archive #65221 dazzler.mod (1.335x the rate, 8-bit, correlation 0.954) |
| 16 | this piece is 100% of | 6982 | probable only | Mod Archive #36504 dazzler2.mod (1.335x the rate, 8-bit, correlation 0.966); Mod Archive #65221 dazzler.mod (1.335x the rate, 8-bit, correlation 0.966) |

### Coastline piece (xm_149C80)

| # | Sample name | Length | Verdict | Sources |
|---|---|---|---|---|
| 0 | 13 | 13768 | exact source | Modland: Protracker/Huezo/wandering - part 2.mod (exact (scan)); Modland: Fasttracker 2/Swallow/dinner and jorma.xm (exact (scan)); Modland: Protracker/Cobolt/necrodancev2.mod (exact (scan)); Modland: Protracker/Kim Berg/necrodance2.mod (exact (scan)); and 16 more |
| 1 | this piece is 100% of | 9320 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/dazzler.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Scorpik/news week 1.mod (same rate, 8-bit, correlation 1.000) |
| 2 | my mind... | 18842 | exact source | Amiga ST-XX disks: ST-58/rsstring (exact); Amiga ST-XX disks: ST-84/scrmlead12pl (exact); Modland: Fasttracker 2/Deepflow/machine lang.xm (exact (scan)); Modland: Protracker/Dump/positivity 02-93.mod (same rate, 8-bit, correlation 1.000); and 270 more |
| 3 | 037.iff | 18842 | composer-made | COMPOSER-MADE. A chord built from the "my mind..." string (ST-58 rsstring): the name is the recipe, root plus 3 and 7 semitones (minor). A solver puts the weight on exactly those three pitches; the exact mix (levels, resampler) was not reproduced. Nothing to source beyond rsstring. |
| 4 | 049.iff | 18842 | composer-made | COMPOSER-MADE. Chord of ST-58 rsstring at 0, 4 and 9 semitones (see 037.iff). |
| 5 | 058.iff | 18842 | composer-made | COMPOSER-MADE. Chord of ST-58 rsstring at 0, 5 and 8 semitones (see 037.iff). |
| 6 | too much homework.) | 8348 | exact source | Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); Amiga ST-XX disks: ST-59/c20.hihatopen (exact, source shorter); Amiga ST-XX disks: ST-61/tip-hhopen (exact, source longer); and 436 more |
| 7 | By Counterpoint of | 10856 | same recording, altered | Counterpoint's f_o_c.mod / fochoice.mod sample 2, same length, correlation 0.989: an edited copy (level or small changes), not byte-identical. Same quality. Other copies: Mod Archive #43028 fochoice.mod (same rate, 8-bit, correlation 0.989); Mod Archive #81705 f_o_c.mod (same rate, 8-bit, correlation 0.989); Modland: Protracker/Counterpoint/foc.mod (same rate, 8-bit, correlation 0.989); Amiga disc: st2a/MODS/F/FREEDOMO.MOD (same rate, 8-bit, correlation 0.989) |
| 8 | it's gonna be a bumpy | 886 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact) |
| 9 | ride ... | 896 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact) |
| 10 | ! all samples by me ! | 1670 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/choices.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Jester/dazzler.mod (same rate, 8-bit, correlation 1.000); and 4 more |
| 11 | (unnamed) | 6482 | exact source | Modland: Impulsetracker/Apple/ghouls'n ghosts.it (exact (scan)); Modland: Protracker/Supernao/coop-Azazel/demotune.add.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Raztaman/dancenumber.mod (same rate, 8-bit, correlation 1.000) |
| 12 | Greets go out to | 23556 | same recording, altered | Counterpoint's f_o_c.mod / fochoice.mod sample 4, same length, correlation 0.989: an edited copy. Same quality. Other copies: Modland: Protracker/Counterpoint/foc.mod (same rate, 8-bit, correlation 0.989); Amiga disc: st2a/MODS/F/FREEDOMO.MOD (same rate, 8-bit, correlation 0.989); Mod Archive #43028 fochoice.mod (same rate, 8-bit, correlation 0.989); Mod Archive #81705 f_o_c.mod (same rate, 8-bit, correlation 0.989) |
| 13 | -- volker tripp -- | 5072 | exact source | Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/dazzler.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Tecon/alt.start i.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Protas/technological zone.mod (same rate, 8-bit, correlation 0.998); and 2 more |
| 14 | pure disco pleasure | 8210 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/dazzler.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Scorpik/news week 1.mod (same rate, 8-bit, correlation 1.000) |

### Strip Mine piece (xm_164B60)

| # | Sample name | Length | Verdict | Sources |
|---|---|---|---|---|
| 0 | (unnamed) | 4506 | exact source | Modland: Protracker/Huezo/wandering - part 2.mod (exact (scan)); Modland: Fasttracker 2/Cerror/polaris's birthday.xm (exact (scan)); Modland: Protracker/Toady/kabukil.mod (exact (scan)); Modland: Fasttracker 2/Cerror/you will be in my chip.xm (exact (scan)); and 10 more |
| 1 | jester of sanity | 4164 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/dazzler.mod (exact (scan)) |
| 2 | Distguit.iff | 24530 | longer copy, not used | ST-58 disk "distchord", same rate, correlation 0.999. The source is longer (31304 samples); the ROM copy is it cut to 24530. Not used: the song never plays past the ROM length, so the swap would sound identical. A WAV of it is in this folder anyway. Other copies: Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); Modland: Fasttracker 2/Deepflow/machine lang.xm (exact (scan)); Amiga ST-XX disks: ST-58/distchord (same rate, 8-bit, correlation 1.000); Modland: Protracker/Unison (DK)/istanbull.mod (same rate, 8-bit, correlation 1.000); and 1 more |
| 3 | "exchange" by gt | 9018 | exact source | Uncle Ben's Protracker module "exchange" (Modland), sample 3, byte-identical. The three Strip Mine names ("exchange" by gt, uncle ben/gigatron, from the demo) are that module's credit lines. Other copies: Modland: Protracker/Uncle Ben/exchange.mod (exact (scan)); Amiga disc: st2b/MODS/E/EXCHANG2.MOD (exact (scan)) |
| 5 | Syn.iff | 11776 | same recording, altered | ST-46 disk "killvoice", same rate, correlation 0.989, slightly longer. Same quality; not an upgrade. Other copies: Amiga ST-XX disks: ST-46/killvoice (same rate, 8-bit, correlation 0.989); Modland: Protracker/Goto80/kjempetrans.mod (same rate, 8-bit, correlation 0.989); Modland: Protracker/Yodelking/sunlight.mod (same rate, 8-bit, correlation 0.989); Modland: Protracker/Hi-Lite/wishbringer.avatar.mod (same rate, 8-bit, correlation 0.989); and 3 more |
| 6 | #uncle ben/gigatron# | 9060 | same recording, altered | Uncle Ben's "exchange", sample 1, byte-identical (see "exchange" by gt). Other copies: Modland: Fasttracker 2/QNeo/kathela jenos -j-s.xm (same rate, 8-bit, correlation 1.000); Modland: Protracker/Uncle Ben/exchange.mod (same rate, 8-bit, correlation 1.000); Amiga disc: st2b/MODS/E/EXCHANG2.MOD (same rate, 8-bit, correlation 1.000) |
| 7 | from the demo | 9108 | same recording, altered | Uncle Ben's "exchange", sample 2, byte-identical (see "exchange" by gt). Other copies: Modland: Fasttracker 2/QNeo/kathela jenos -j-s.xm (same rate, 8-bit, correlation 1.000); Modland: Protracker/Uncle Ben/exchange.mod (same rate, 8-bit, correlation 1.000); Amiga disc: st2b/MODS/E/EXCHANG2.MOD (same rate, 8-bit, correlation 1.000) |
| 8 | it's gonna be a bumpy | 886 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact) |
| 9 | ride ... | 896 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact) |
| 10 | ! all samples by me ! | 1670 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/choices.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Jester/dazzler.mod (same rate, 8-bit, correlation 1.000); and 4 more |
| 11 | this piece is 100% of | 9320 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/dazzler.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Scorpik/news week 1.mod (same rate, 8-bit, correlation 1.000) |
| 12 | pure disco pleasure | 8210 | exact source | Mod Archive #36504 dazzler2.mod (exact); Mod Archive #65221 dazzler.mod (exact); Modland: Protracker/Jester/dazzler.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Scorpik/news week 1.mod (same rate, 8-bit, correlation 1.000) |
| 13 | send all your secret | 11189 | probable only | PROBABLE only. dazzler2.mod (Mod Archive #36504) sample 7, 4 semitones higher (correlation 0.94, 0.90 over the full length). Not strong enough to call the same recording; not used. Other copies: Mod Archive #36504 dazzler2.mod (1.260x the rate, 8-bit, correlation 0.944) |
| 16 | (unnamed) | 4400 | exact source | Modland: Fasttracker 2/Caaher/old skool vs new skool.xm (exact (scan)); Modland: Fasttracker 2/DNA Trance/exp into the unknown.xm (exact (scan)); Modland: Fasttracker 2/Foo/hehf.xm (exact (scan)); Modland: Fasttracker 2/Neotraxx/recording.xm (exact (scan)); and 287 more |
| 17 | (unnamed) | 3440 | exact source | Modland: Fasttracker 2/Shifter/unreal dreamz.xm (exact (scan)); Modland: Fasttracker 2/- unknown/boundless universe.xm (exact (scan)); Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); Modland: Fasttracker 2/DJ Schnee/frozen mind.xm (exact (scan)); and 86 more |
| 18 | (unnamed) | 4864 | exact source | Modland: Fasttracker 2/- unknown/boundless universe.xm (exact (scan)); Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); Modland: Fasttracker 2/DJ Schnee/frozen mind.xm (exact (scan)); Modland: Fasttracker 2/OsO/dusty sky.xm (exact (scan)); and 52 more |
| 19 | .% FRESHTRANCE %. | 23727 | better copy, used | UPGRADE, in use. lotus_2_completition_music_remix_2.xm (Mod Archive #135802) instrument 16, 1.68 times the rate (correlation 0.99). Other copies: Modland: Fasttracker 2/McBarn/mr ass.xm (1.682x the rate, 8-bit, correlation 0.985); Modland: Impulsetracker/Arty/temptations.it (1.682x the rate, 8-bit, correlation 0.985); Modland: Fasttracker 2/Barry Leitch/twisted edge - snowboard 02.xm (1.682x the rate, 8-bit, correlation 0.985); Modland: Impulsetracker/Arty/cybernet.it (1.682x the rate, 8-bit, correlation 0.985); and 25 more |
| 20 | ST-28:yesbunk | 23699 | same recording, altered | The ST-28 disk's "yesbunk" (also in switchblade_2_title.mod and planet_fall_boom.mod). The ROM copy is an edited, shortened version at the same rate (correlation 0.976). Same 8-bit quality; not an upgrade. Other copies: Mod Archive #129344 planet_fall_boom.mod (same rate, 8-bit, correlation 0.973); Mod Archive #129351 planetfallboom_long.mod (same rate, 8-bit, correlation 0.973); Mod Archive #82797 switchblade_2_title.mod (same rate, 8-bit, correlation 0.973); Mod Archive #110603 moochiesloadt.mod (same rate, 8-bit, correlation 0.973) |

### Jungle piece (xm_17FD10)

| # | Sample name | Length | Verdict | Sources |
|---|---|---|---|---|
| 0 | grandpiano3 | 9813 | better copy, used | UPGRADE, in use. The ST-86 disk sample "grandpiano3"; the ROM copy is it pitched up 3 semitones (rate x1.189) and amplified about 1.8x. The "3" in the name is the semitone shift. Other copies: Modland: Screamtracker 3/Hawk (AT)/the gate of iron.s3m (1.189x the rate, 8-bit, correlation 0.979); Modland: Protracker/Falcon (DE)/death-box.mod (1.189x the rate, 8-bit, correlation 0.979); Amiga disc: st1a/SAMPLES/VARIOUS/P/PIANO5.SND (1.189x the rate, 8-bit, correlation 0.979); Amiga disc: st2b/MODS/D/DEATHBO.MOD (1.189x the rate, 8-bit, correlation 0.979); and 105 more |
| 1 | grandpiano4 | 10645 | better copy, used | UPGRADE, in use. The ST-86 disk sample "grandpiano4"; the ROM copy is it pitched up 4 semitones (rate x1.260) and amplified about 1.8x. Other copies: Mod Archive #177541 bronzes.mod (1.260x the rate, 8-bit, correlation 0.988); Modland: Protracker/Falcon (DE)/death-box.mod (1.260x the rate, 8-bit, correlation 0.968); Modland: Protracker/Etrimon/everything blissed.mod (1.260x the rate, 8-bit, correlation 0.968); Amiga disc: st2b/MODS/D/DEATHBO.MOD (1.260x the rate, 8-bit, correlation 0.968); and 123 more |
| 2 | xxbass | 11292 | exact source | Same sample as the title piece's xxbass (see there). XXBASS.WAV in x4songa.it is a different recording. Other copies: 135113.bin (exact); 135549.bin (exact); Mod Archive #155390 anarchy-2-1629.mod (exact); Modland: Fasttracker_2_Barry_Leitch_twisted_edge_-_snowboard_06.xm (exact); and 13 more |
| 3 | for more MODs in the | 23028 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 281 more |
| 4 | near future!!!! | 8296 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 171 more |
| 5 | (unnamed) | 8336 | exact source | 135355.bin (exact); 136239.bin (exact); Mod Archive #43028 fochoice.mod (exact); Mod Archive #81705 f_o_c.mod (exact); and 488 more |
| 6 | st-34:34hihat | 1870 | exact source | Mod Archive #177541 bronzes.mod (exact, source longer); Mod Archive #47057 KLISJE.MOD (exact); Mod Archive #93055 its_moby.mod (exact); Amiga ST-XX disks: ST-86/hihat32 (exact); and 643 more |
| 7 | ST-96:mobypiano | 25500 | exact source | Mod Archive #177541 bronzes.mod (exact); Mod Archive #47057 KLISJE.MOD (exact); Mod Archive #93055 its_moby.mod (exact); Amiga ST-XX disks: ST-86/mobypiano (exact); and 346 more |
| 8 | ST-96:mobysnare | 5436 | exact source | Mod Archive #177541 bronzes.mod (exact); Mod Archive #47057 KLISJE.MOD (exact); Mod Archive #93055 its_moby.mod (exact); Amiga ST-XX disks: ST-86/mobysnare (exact); and 102 more |
| 9 | st-87:guitar90 falsk | 22676 | exact source | Mod Archive #162205 gpiany4.mod (exact); Mod Archive #177541 bronzes.mod (exact); Mod Archive #47057 KLISJE.MOD (exact); Mod Archive #93055 its_moby.mod (exact); and 323 more |
| 10 | wack10 | 5144 | exact source | Modland: Fasttracker 2/ZedFox/holidays.xm (exact (scan)); Modland: Protracker/Arpegiator/wackomania.mod (same rate, 8-bit, correlation 1.000) |
| 11 | (audio-cd) ch.drm3 | 4400 | exact source | Mod Archive #161014 oistein_eide_-_flee_heart.mod (exact); Amiga ST-XX disks: ST-60/(acd) ch.drm3 (exact); Modland: Protracker/King Arthur/level2.mod (same rate, 8-bit, correlation 1.000); Modland: Protracker/Kukiz/modo3.mod (same rate, 8-bit, correlation 1.000); and 22 more |
| 12 | mon.().d20-ohat.a1 | 11936 | exact source | Mod Archive #161014 oistein_eide_-_flee_heart.mod (exact); Amiga ST-XX disks: ST-60/(1).d20-ohat.a1 (exact); Amiga ST-XX disks: ST-59/tech-hihatcrash (exact); Modland: Protracker/Chrylian/mystery land.mod (same rate, 8-bit, correlation 1.000); and 19 more |
| 13 | guru--chord2 | 10642 | exact source | Mod Archive #135103 twisted-edge-snowboard-05.xm (exact); Modland: Fasttracker_2_Barry_Leitch_twisted_edge_-_snowboard_05.xm (exact); Modland: Fasttracker 2/Barry Leitch/twisted edge - snowboard 05.xm (exact (scan)); Modland: Protracker/Barry Leitch/zonewarrior-highscore.mod (exact (scan)) |
| 14 | guru-chord4 | 10482 | exact source | Mod Archive #135103 twisted-edge-snowboard-05.xm (exact); Modland: Fasttracker_2_Barry_Leitch_twisted_edge_-_snowboard_05.xm (exact); Modland: Fasttracker 2/Barry Leitch/twisted edge - snowboard 05.xm (same rate, 8-bit, correlation 1.000) |

