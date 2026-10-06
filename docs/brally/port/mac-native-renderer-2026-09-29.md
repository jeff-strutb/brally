# Mac native renderer

*Recorded 2026-09-29.*

> Mac port NATIVE_RENDERER.md phases 1-6 landed 2026-09-29; what is native, what is not, and the traps found

ports/brally-wasm/NATIVE_RENDERER.md phases 1-6 were implemented 2026-09-29 (commits 6c9c5ca3..872baf0d). The spec's section 5 holds the measured results; read it before touching the 32-bit lane's renderer.

**Why:** the project lead asked for all phases; later sessions will be asked to go further ("clean, proper modern native").

**How to apply:**
- Still not native: render-mode / combine / texture leaves reach Metal through the Glide state calls in host_glide.m (the spec says so). The game's CPU clipper is still live for 0xE3/0xE4 textured rects.
- Latency is bounded by WindowServer's compositing depth (1-3 refreshes, external); CAMetalDisplayLink and presentDrawable:atTime: measured worse. Don't re-tune pacing constants ([native-renderer-not-hacks](native-renderer-not-hacks.md)).
- Runs are deterministic under BR_VCLOCK only for matched conditions: compare A/B builds as concurrent pairs, not against an old reference dump (a flawed reference cost an hour).
- The wasm lane's C++ used Itanium vtable slots against the original MSVC vtables; ccmark.py now takes slots and member pointers from an i686-pc-windows-msvc view. Suspect this class first for any C++ crash in the port.
- Open question for the project lead: Mac arrow instead of the game's cursor sprite in menus.
