# Music in a port — OPEN DECISION, ASK BEFORE BUILDING

**No port plays music yet, and this is deliberate. Nothing in `ports/` reads
either soundtrack export.**

> **If you are here because music porting has come up: STOP and ask the user.**
> The intent is a **player-facing choice between the PC and N64 soundtracks** —
> PC plays the disc's CD audio, N64 plays the tracker modules. That intent is
> settled. *How* to build it is not, and the user wants to decide it with the
> findings below in front of them rather than have a design chosen for them.
> The questions to put to them are at the bottom.

Recorded 2026-09-03. Everything here was measured, not assumed; the working is
in `docs/audio-xm-notes.md`.

## Two soundtracks, two very different sources

| | PC (Boss Rally) | N64 (Top Gear Rally) |
|---|---|---|
| what it is | Redbook CD audio, tracks 2–13 of the game disc | six FastTracker II modules, zlib-packed in the ROM |
| extractor | `tools/extract_cdaudio.py` | `tools/extract_xm.py` |
| lands in | `testdata/music_cd` | `testdata/music_xm` |
| pieces | 12 tracks | 6 modules |
| size, lossless | ~319 MB | **1.24 MB of modules**, or ~98 MB rendered to FLAC |
| player needed | a FLAC decoder | a tracker replayer, or pre-rendered FLAC |
| loops | finite recordings; needs a wrap point, probably with a seam | seamlessly and forever, by construction |

**Both exports are faithful rips.** Neither applies a fade, invents an ending,
or makes a musical decision, and both record a PCM hash. Whatever the port does
with looping, crossfading or level, it does at *playback* — the files carry the
information, not a baked-in interpretation of it. Do not change that to make a
port easier.

The XM soundtrack is the **N64 twin's** music. Offering it in a PC port is a
deliberate addition, not a restoration — worth being explicit about in whatever
UI exposes the choice.

## The playback question this hangs on

The N64 side can be played two ways, and the choice is real:

**Pre-rendered FLAC** (what the export produces today). No new runtime
dependency, and since the rip is now one faithful pass with the loop point
recorded as `restart_frame`, looping it *is* possible — seek back to that frame
at the end. It costs 98 MB instead of 1.24 MB, and the seam is a hard cut where
a tracker would have continuity. Note the loop points are not all zero: four of
the six modules have an intro and return to a point partway in, as late as
69.1s.

**Runtime XM playback via libopenmpt.** 1.24 MB, loops correctly with no effort,
and — the part that matters — libopenmpt is already the oracle the project scores
its own renderer against, so the port would sound exactly like the reference by
construction rather than by continued effort. BSD-3-Clause (verified against the
installed copy), stable C API, pull-based streaming, infinite repeat mode.

**Do not ship `tools/xm_render.c` as the port's player.** It is a batch renderer
that writes a whole file, it would need restructuring into a real-time callback,
and it scores 0.95–0.99 against libopenmpt — shipping it means shipping its
known-wrong vibrato scaling. It earns its keep as build tooling and as the thing
the oracle measures. Not as a runtime.

## Four things that will bite, whatever is chosen

**1. Availability is not preference.** CD audio needs the player's disc image;
the modules need their N64 ROM. Many people will have exactly one. The setting
therefore has three states — chosen, not chosen, **not present** — and first run
has to default from what actually got extracted, not from a stored preference.

**2. The two soundtracks do not line up track-for-track.** Twelve CD tracks
against six modules. Music selection cannot be an index into a list; it needs a
cue map per soundtrack, keyed by game state. *What the game asks for at each
moment is a decomp question* — the PC music-cue logic will answer it for free as
that code lands. Waiting for it beats guessing a mapping now.

**3. Loudness must be matched across the two.** CD audio is mastered near full
scale; the modules render nowhere near it. Switching soundtracks mid-session
would jump in volume. One calibration constant per soundtrack, measured once.
This is where the export's level work pays off — not the shared gain itself
(libopenmpt applies its own mixing), but the fact that the modules' true composed
peaks are now known: they span 1.916 to 2.938, about 3.7 dB of composed range
that per-track normalisation used to flatten.

**4. Do not push playback decisions back into the exports.** Both are 1:1 rips
by policy — no fade, no invented ending, nothing assumed. If the port wants a
fade, a crossfade or a loop, it does that at playback. A decision baked into a
file cannot be undone later, and the XM export already lost every module's loop
structure once by rendering two passes and fading out.

## Shape, if it helps the conversation

Both backends reduce to the same thing: fill a buffer with interleaved stereo
PCM. The host audio layer does not need to know which is playing. One interface,
libopenmpt behind one and a FLAC decoder behind the other — the asymmetries above
live in selection, looping and levels, not in the audio path.

## Ask the user these

1. Runtime XM via libopenmpt, or pre-rendered FLAC for the N64 soundtrack?
   (Recommendation on the evidence: libopenmpt. 150:1 on size, correct looping,
   and it *is* the reference.)
2. Is a third-party library acceptable in the port at all? `ports/` is not
   byte-matched so nothing in the decomp rules forbids it, but it is the first
   one and that is the user's call.
3. What should happen when only one soundtrack's source media is present —
   silently default to it, or surface it in the UI?
4. Wait for the decomp's music-cue logic before designing selection, or ship an
   interim mapping?
5. Should the port's audio work start at all before the renderer's open leads
   (vibrato depth scaling, Amiga finetune — `docs/audio-xm-notes.md`) are closed?
   They do not block libopenmpt playback, only our own renderer.
