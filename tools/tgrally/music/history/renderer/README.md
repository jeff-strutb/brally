# Stereo remaster renderer (Jungle)

1. `cc -O2 -o xmlog xmlog.c -lm`, then `XM_EVENTLOG=events.csv ./xmlog --rate 48000 --passes 1 --fade-ms 0 module.xm out.wav`
   logs the performance (every channel, every tick: note start, playback rate, final volume, pan).
2. `cc -O3 -shared -fPIC -o libsynth.dylib synth.c` builds the synth engine.
3. `jungle_render.py` plays the performance with the instruments below into one stereo track per part.
4. `mix.py tracks.npz out.wav` levels each track to the original's K-weighted arrangement balance, applies
   the per-track chain, shared reverb, master tonal target, bus glue and a limiter to -14 LUFS.

Sources (download separately into `lib/`): Salamander Grand Piano V3 48k/24 (CC-BY 3.0),
FreePats FSBS clean electric guitar DI (CC0), DRSKit 2.1 (CC-BY 4.0). Amp: NAM captures from
pelennor2170/NAM_models (Helga B 5150 BlockLetter). Cab: Science Amplification 4x12 V30 SM57 IR.
Chord shapes and tuning: transcribed from the ROM samples, detuning removed (A440).
