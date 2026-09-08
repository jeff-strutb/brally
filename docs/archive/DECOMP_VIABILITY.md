# Boss Rally (PC, 1999) Decompilation Viability Analysis

## What the disc actually contains

`BossRally.BIN/.cue` is a MODE1/2352 data track (58,143 sectors, about 130 MB)
followed by **12 Redbook CD-audio tracks**, which are the PC soundtrack. The
data track is extracted to `work/BossRally.iso` (ISO9660, volume label `Boss
Rally`).

All binaries are x86, MSVC **linker 5.0** (Visual C++ 5.0), built March 1999.

| File | .text | Distinct call targets | Role |
|---|---|---|---|
| **BRD3D.dll** | 581,632 B | **~1,475** | Whole game plus DirectDraw renderer |
| **BRGlide.dll** | 481,280 B | ~1,075 | Whole game plus 3dfx Glide renderer |
| Boot.exe | 96,768 B | ~579 | Launcher/config (C++, MFC-ish) |
| BossRally.exe | 23,552 B | | Thin loader |
| SetVideo.exe | 36,864 B | | Video mode setup |

**The game is the DLL.** `BRD3D.dll` and `BRGlide.dll` each export exactly one
symbol: `RallyMain`. The EXE picks a renderer DLL and calls it.

Imports: `DDRAW`, `DINPUT`, `DPLAYX`, `MSACM32`, `WINMM`, `ole32` (D3D build);
`glide2x` replaces `DDRAW` in the Glide build.

## Factors in favour

1. **It is C, not C++.** No RTTI, no mangled names, no vtables anywhere in the
   game DLLs (`.?AV` type descriptors appear only in `Boot.exe`, the launcher).
   C decompiles far more cleanly than C++ and needs no class-hierarchy
   reconstruction.
2. **The compiler is obtainable.** MSVC 5.0/6.0 still exist and their codegen is
   well characterised. A *matching* decomp, meaning source that recompiles to a
   byte-identical DLL, is realistic here, unlike consoles with exotic
   proprietary compilers.
3. **Tractable size.** About 1,475 functions. For scale, the Super Mario 64
   decomp was about 2,500 and Diablo's about 3,000; both were completed.
4. **One entry point, clean boundary.** A single exported `RallyMain` and a
   renderer-abstraction seam that *already exists*: the D3D and Glide builds are
   the same game code against two backends. Adding a third (modern) backend
   follows a seam the original developers already cut.
5. **Two builds cross-validate.** Functions shared between BRD3D and BRGlide can
   be diffed against each other to separate game logic from renderer code.
6. **Assets need no reversing.** `Images/` and `Paint/` are plain Windows BMPs,
   `sfx/` is plain RIFF/WAV, `cargfx/` is `.ci4` plus `.lut4` (raw N64 CI4
   textures and 16-entry RGBA5551 palettes), plus `.rca` car models, `.pod`,
   `.img`, `.dat`. Nothing is encrypted or packed.
7. **Strings survive and are function-scoped.** For example `CHK_FReadOpen():
   error opening file %s.`, `DDraw_DoInit: Bitmap %d failed to load!`, `Error:
   Track header size mismatch(%d != %d)`, and the DirectInput/DirectPlay error
   paths. These hand you real internal function names and module boundaries, a
   large head start on naming.
8. **N64 SDK leakage.** The string `Error: guFrustumF(): unable to compute
   matrix` proves the PC port carried over libultra's `gu*` math library.
   libultra is publicly documented, so those math functions can be matched
   against known source rather than reverse-engineered.
9. **The N64 ROM is a second reference implementation** of the same game,
   sharing asset conventions (the PC port ships N64-format `.ci4`/`.lut4`
   verbatim).
10. **Static CRT in BRD3D.dll.** CRT code is identifiable by signature and can
    be excluded from the work rather than decompiled.

## Factors against

1. **No symbols at all.** No debug directory, no CodeView record, no PDB path
   string in any binary. Every name must be recovered by hand.
2. **Optimised build with frame-pointer omission.** Only 129 classic `push ebp;
   mov ebp,esp` prologues across 581 KB, so most functions index locals off
   `esp`. This is the single biggest drag on decompiler output quality and on
   recovering local layouts.
3. **Huge inferred global state.** `.data` has a 25 MB virtual size against 185
   KB of raw data, meaning about 25 MB of BSS. Global structure layout has to be
   inferred from access patterns, which is the slow part of any decomp.
4. **Dead APIs.** DirectDraw (DX6-era) and Glide are both gone. Any rebuild that
   runs on a modern OS needs a new backend regardless of how the decomp goes.
5. **Likely hand-written asm or MMX** in the software rasteriser and math paths,
   which decompiles poorly and needs manual translation.

## Recommendation

*Historical note: this section is the original viability write-up. The project
has since committed to the full matching decomp described in Tier 3; the tiered
advice below is kept as the record of how that decision was reached.*

**Tier 1: DLL shimming (days to weeks).** Because the game is a single DLL with
one export, you can write a proxy DLL that forwards `RallyMain` and hot-patches
individual functions. Combined with the plain-BMP/WAV assets, a large fraction
of the "fun tweaks" category (menu art, text, car stats, track data, UI
behaviour) is reachable *without decompiling anything*. It gives working results
immediately.

**Tier 2: targeted decompilation (weeks to months).** Decompile only the
subsystems you want to change, in Ghidra, and re-implement them in the shim. The
error strings give you module boundaries to carve along (file I/O, track
loading, DirectInput, DirectPlay).

**Tier 3: full matching decomp (est. 800 to 2,000 hours solo).** Genuinely
feasible here. This is a well-shaped target, better than most, and worth it when
the goal is a permanent, portable, community source tree. Target `BRD3D.dll`
with MSVC 5.0 and verify with `objdiff`/`asm-differ`-style tooling.

**Tooling:** Ghidra (free, best-in-class for this) or IDA. Avoid RetDec; it will
not produce recompilable code for a target this size.

**Verdict: viable, and unusually favourable for a 1999 title.** C rather than
C++, an obtainable compiler, one clean export, open asset formats, dual builds
for cross-checking, and a same-game N64 ROM as a reference. The absence of any
debug symbols and the roughly 25 MB of BSS are the real costs.
