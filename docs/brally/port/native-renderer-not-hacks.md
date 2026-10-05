# Feedback native renderer not hacks

*Recorded 2026-09-29.*

> Mac port -- build a proper Mac-native renderer/frame loop from the decomp source; do not tune the Glide-emulation lane by repeated run-and-measure

2026-09-29: while I was tuning frame pacing/latency in the wasm lane's Glide->Metal
shim by launching the game over and over with different constants, the project lead said:
"Stop guessing and running the game over and over... rewrite the underlying renderer
to be properly Mac native. You have the true source now. Don't hack the original to
just 'run', do it right."

**Why:** the decomp is complete, so emulating Glide/DirectX underneath the original
binary's behaviour is the wrong layer; trial-and-error tuning of that shim wastes time
and pops windows over the project lead's play session.

**How to apply:** for Mac port quality issues (latency, pacing, resolution, input),
design the native path in ports/macos from the source (renderer that consumes the
game's display lists directly, a frame loop the port owns) -- plan from reading code,
not from repeated game launches. Never launch windowed test runs while the project lead may be
playing without saying so. Related: [macos-port-32bit-wasm-lane](macos-port-32bit-wasm-lane.md),
[port-never-touches-decomp](../decomp/rules/port-never-touches-decomp.md), no-token-thrashing.
