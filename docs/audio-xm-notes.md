# N64 soundtrack export — XM notes (2026-09-03)

The port plays the Top Gear Rally soundtrack as lossless audio. The N64 build
stores it as six FastTracker II modules, zlib-packed in the ROM;
`tools/extract_xm.py` finds and unpacks them, `tools/xm_render.c` renders them,
ffmpeg encodes FLAC. Output goes to **`testdata/music_xm`** — the directory
`setup.sh` uses, and therefore the one everything downstream reads. Exporting
anywhere else leaves the stale copy in place and looks, from outside, exactly
like a run that did nothing. That happened this session.

Nothing here is committed; the export is regenerated on each machine from that
machine's own ROM.

## There is no soundfont, and no sample set

The samples live inside the .xm modules. A "the instruments sound wrong"
report is therefore always a **replayer** bug and never an asset problem —
there is no external bank to point at, no General MIDI, nothing to swap.

## What the modules actually contain

Measured across all six, not assumed:

| | |
|---|---|
| effects used | `1` `2` `3` `4` `A` `C` `E9` `F` — and nothing else |
| volume column | set-volume (`0x10`..`0x5F`) only |
| volume envelopes | **none**, in any instrument of any module |
| note-off | never used |
| samples | all 8-bit; loop type none or forward; no ping-pong, no 16-bit |
| frequency table | linear in four modules, Amiga in two |
| tracker | `FastTracker v2.00`, format 1.04 |

`xm_render.c` counts any effect it does not implement and the extractor fails
loudly on a non-empty count, so a MISSING effect cannot ship silently.

## The trap: a clean census says nothing about correctness

That check was clean the entire time two of the six tracks played **two octaves
sharp**. The Amiga branch of `period_of_note()` used `floor(note/12) - 2` where
it needed `- 4`. An effect census sees which effects are used, never whether
they are implemented right, and this is the failure mode a hand-written
replayer actually has.

The invariant that catches it — the two frequency tables must agree on pitch
for every note, since Amiga mode is a different tuning resolution, not a
transposition:

    8363 * 1712 / amiga(n)  ==  8363 * 2^((4608 - (7680 - n*64)) / 768)

## The oracle: `tools/xm_oracle.py`

Scores `xm_render.c` against **libopenmpt** — OpenMPT's replayer, validated
against FastTracker II itself, and what MilkyTracker and VLC use.

    brew install libopenmpt
    python3 tools/extract_xm.py "<rom>" testdata/music_xm --keep-xm
    python3 tools/xm_oracle.py testdata/music_xm/*.xm
    python3 tools/xm_oracle.py --per-channel testdata/music_xm/xm_0EBC00.xm

Design points worth not re-deriving:

- The two renders are matched in **RMS, never in peak**. libopenmpt hard-clips
  into int16 (0.03% of samples on the 16-channel module) while `xm_render`
  scales to fit, so their peaks are not the same kind of number and a
  peak-match reports a crest-factor artefact as a fidelity gap.
- Scoring is **windowed correlation**, not a byte diff. The two mixers differ
  in rounding, so exact equality is not on offer. What windows buy is
  localisation: a bad window carries a timestamp, and the timestamp says which
  rows to read.
- `--per-channel` blanks every channel but one — **keeping `Bxx`, `Dxx` and
  `Fxx`**, which steer the whole song. Strip those with the rest and the
  isolated render plays at a different tempo from the mix it is meant to
  explain.
- `--lag` fits a sample offset per bad window. A lag that grows over time is a
  pitch or tempo error; a flat lag with poor correlation is not, and that
  distinction ruled out timing on the first pass here.

Baseline medians over 0.25s windows, which any change must beat:

| module | table | median | was, before the octave fix |
|---|---|---|---|
| `xm_0EBC00` | linear | 0.9903 | 0.9903 |
| `xm_113660` | linear | 0.9742 | 0.9742 |
| `xm_12EAB0` | linear | 0.9674 | 0.9674 |
| `xm_149C80` | **amiga** | 0.9515 | **0.0142** |
| `xm_164B60` | linear | 0.9921 | 0.9921 |
| `xm_17FD10` | **amiga** | 0.9525 | **−0.0408** |

## Open, ranked

1. **Vibrato depth scaling is a guess.** The offset is normalised as
   `2.0f*sine*depth/15` and then multiplied by the same `16.0f` in both the
   linear and the Amiga period domains. One constant cannot be right in two
   unit systems, and the two lowest scorers are the two Amiga modules.
2. **Amiga finetune.** We fold finetune into a continuous note index and
   interpolate between adjacent *semitone* periods; FT2 interpolates within a
   finetune table. Same suspects.

## Dead ends — do not re-run

- **Fadeout rate** (`/32768` vs `/65536`): moves every score by *exactly* zero.
  No module uses note-off, so the fadeout path is never entered.
- **Envelope sustain-vs-loop ordering** (FT2 tests sustain first; we test the
  loop first): latent for the same reason — no instrument in any of the six
  enables a volume envelope at all.

Both are still worth fixing if this renderer is ever pointed at another module,
but neither can move a number on *these* six.

## Levels

All six take **one shared gain**, from the loudest module's peak.

XM has no module-level master volume: the composed level is carried entirely by
sample volumes, the volume column, `Cxx` and envelopes, so the mix level *is*
the composed level. Normalising each track to its own peak — which is what this
did until now — flattens six deliberately unequal tracks into equal loudness.
The measured peaks span 1.916 to 2.938, about 3.7 dB of real composed dynamic
range; after the change only the loudest track lands at −1 dBFS and the others
sit 2.9 to 4.7 dB below it.

`--per-track-gain` restores the old behaviour. `xm_render --measure` runs pass 1
alone, which is what lets the extractor learn the whole set's peaks before it
has to choose a gain.

## Host dependency

`openmpt123` (from libopenmpt) is needed only by the oracle, never by the
export — it sits alongside the ffmpeg the extractor already requires. Rule 5
covers the MSVC/Wine toolchain, which is staged in-tree; this is build-time
asset tooling and is not part of it.
