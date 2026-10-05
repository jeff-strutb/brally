# Exports are 1 to 1

*Recorded 2026-09-03.*

> RULE - every asset extractor is a faithful 1:1 rip that assumes nothing; playback decisions belong at playback, never baked into a file.

**RULE, stated 2026-09-03:** "Our export should be 1:1 not make assumptions
on ANYTHING." Said of the CD-audio export, but meant generally - it applies to
every extractor in `tools/`.

No fades. No invented endings. No per-file normalisation. No trimming, no
padding, no "it sounds better this way". If an edit is genuinely unavoidable
(rendering a tracker module to integer PCM needs *some* scale factor because the
mix sums past unity), make it uniform across the set, record it exactly in the
manifest so it inverts, and say in the docstring why it could not be avoided.
Record a `pcm_sha256` per track so a re-run can be shown to have changed nothing.

**Why:** an export that makes a musical decision has destroyed the information
needed to make a different one later. This is not hypothetical - the XM export
rendered two passes and faded out over four seconds, and four of the six modules
restart at an order position *partway into the song*, so the fade wrote over the
loop and the rip could not be looped at all. One module returns to 69.1s. That
loss was invisible until someone went looking for the loop point.

**How to apply:** `tools/extract_cdaudio.py` is the reference implementation  - 
it copies sectors, decides nothing, hashes the PCM. Anything that renders rather
than copies (`tools/extract_xm.py`) has to justify each departure from that in
its docstring. Convenience options like `--passes` and `--fade-ms` may exist for
making a standalone listening copy, but they must never be the default: a
listening copy is an opinion and a rip is not. Push every playback decision  - 
looping, crossfade, level matching between soundtracks - into the player, and
keep the file carrying the *information* instead of an interpretation of it.

See [xm-soundtrack-oracle](../../../tgrally/music/xm-soundtrack-oracle.md) for the audio-specific findings and
`docs/audio-xm-notes.md` for the working.
