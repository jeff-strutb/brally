# Vc4x version hypothesis refuted

*Recorded 2026-09-22.*

> BrCarStateEncodeDelta's residue is NOT a VC4.x version gap - all 4.0/4.1/4.2 builds emit identical bytes; it's a register-colouring wall

The bitstream-net family (BrCarStateEncodeDelta 0x10006BA0) was long theorised
to need a 4.x compiler *below* 4.2 to go byte-exact ([vc42-is-the-real-compiler](vc42-is-the-real-compiler.md)).
On 2026-09-21 the project lead supplied the actual VC++ 4.0/4.1/4.2 media (reference/msvc/*.7z),
so I tested it directly. **The version hypothesis is refuted.**

Staged under tools/toolchains/msvc40, tools/toolchains/msvc41 (gitignored, beside tools/toolchains/msvc42). All /O2:
- VC4.0 Pro `cl 10.00.5270` → 913 B
- VC4.0 Std `cl 10.00.6002` → 1091 B (frame-based, worse)
- VC4.1 `cl 10.10.6038` → 913 B - **byte-identical to 4.0 Pro**
- VC4.2 `cl 10.20.6166` → 913 B - **byte-identical to 4.0 Pro**

The three professional optimisers emit the SAME bytes; none reproduces the
original's 925-B prologue. Also dead across /Ox /O1 /O2y /Oxs /Oz /Og/Os and the
/G-series. The 12-byte residue is a **register-colouring wall**, not a version
gap: the original saves `ebx,ebp,esi,edi` EAGERLY in register-number order at
entry; every available 4.x saves them lazily (`push esi; mov esi,arg; …; push ebp`),
which drives a whole-function ebx↔ebp rotation ([register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md)).

**Do not chase more 4.x point releases for this residue.** Reachable disposition
is T3 via the A5 oracle (the rotation is behaviour-neutral). Extraction path for
the media: `bsdtar` (libarchive reads .7z and the ISO inside) - no host 7z needed;
compiler tree is `VC41/MSDEV/BIN` (4.1) / `MSDEV/BIN` (4.0 disc1) → stage bin/include/lib.
See the in-file dossier (STATE 2026-09-21). [trampoline-goto-t4-lever](../levers/trampoline-goto-t4-lever.md) is the
counter-example (a "compiler wall" that WAS source-reachable) - but this one is genuinely colouring.
