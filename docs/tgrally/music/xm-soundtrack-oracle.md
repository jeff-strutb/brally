# Xm soundtrack oracle

*Recorded 2026-09-03.*

> The N64 XM->FLAC export is scored against libopenmpt by tools/xm_oracle.py; there is no soundfont, and vibrato scaling is the top open inaccuracy.

The Top Gear Rally soundtrack export (`tools/extract_xm.py`) renders six
FastTracker II modules out of the N64 ROM with **our own** replayer,
`tools/xm_render.c`. There is no soundfont and no external sample set - the
samples live inside the .xm modules - so any "wrong instruments" complaint is a
replayer bug, never an asset problem.

**The oracle is `tools/xm_oracle.py`** (added 2026-09-03). It scores
`xm_render.c` against libopenmpt (`brew install libopenmpt` → `openmpt123`),
which is what MilkyTracker and VLC use, as windowed correlation with the two
renders RMS-matched - never peak-matched, because libopenmpt hard-clips into
int16 while `xm_render` scales to fit. `--per-channel` blanks every channel but
one (keeping Bxx/Dxx/Fxx, which steer the whole song) to turn "something is
wrong around t=8s" into "channel 5 is wrong".

**Why it had to exist:** the renderer already failed loudly on *missing*
effects, and that check was clean the entire time two of the six tracks played
two octaves sharp. An effect census cannot see a wrongly implemented effect.
The bug was `period_of_note`'s Amiga branch using `floor(note/12) - 2` instead
of `- 4`. Invariant to test any change against: the Amiga and linear tables must
agree on frequency for every note.

Baseline medians after the fix (0.25s windows), the number any change must beat:

    xm_0EBC00 0.9903  xm_113660 0.9742  xm_12EAB0 0.9674
    xm_149C80 0.9515  xm_164B60 0.9921  xm_17FD10 0.9525

**Top open lead: vibrato depth scaling.** `vibrato_offset` is normalised as
`2.0f*sine*depth/15` then multiplied by the same `16.0f` in both the linear and
the Amiga period domains - one constant cannot be right in two unit systems, and
the two lowest scorers are the two Amiga modules. Amiga finetune interpolation
is the second lead (we interpolate between semitone periods; FT2 interpolates
within a finetune table).

**Dead ends already probed, do not re-run:** the fadeout rate (`/32768` vs
`/65536`) moves the score by exactly zero, and so would the envelope
sustain-vs-loop ordering - verified across all six modules, **none** of their
instruments enables a volume envelope, none uses note-off, all samples are 8-bit
and loop forward or not at all.

**Levels:** all six get ONE shared gain from the loudest module's peak
(`--per-track-gain` opts out). XM carries no module-level master volume, so the
mix level is the composed level; normalising each track to its own peak flattens
six deliberately unequal tracks. Peaks span 1.916..2.938, i.e. ~3.7 dB of real
composed dynamic range.

Related: [tgr-n64-viability](../decomp/tgr-n64-viability.md) (the same ROM is the source oracle for matching).
