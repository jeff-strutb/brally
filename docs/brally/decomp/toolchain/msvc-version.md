# Msvc version

*Recorded 2026-08-19.*

> Original binaries compiled with MSVC 5.0 (Visual Studio 97), linker 5.0, March 1999. Needed for matching decomp builds.

Original binaries (BRD3D.dll, BRGlide.dll, BRally.exe) all compiled with **MSVC 5.0** (Visual Studio 97).

Evidence:
- Linker version 5.0 in PE optional header across all three binaries
- No Rich header (pre-VS2002)
- Imports MSVCRT.dll (not versioned MSVCRxx.dll)
- PE timestamp: March 30, 1999

**Why:** The matching build must use this exact compiler to produce bit-identical output.
The DLLs also import: KERNEL32, USER32, GDI32, ADVAPI32, ole32, DDRAW, DINPUT, DPLAYX, MSACM32, WINMM.

**How to apply:** Get VC5's `cl.exe` running on the Windows machine. Compile decomped C with it, diff `.obj` output against bytes extracted from original DLL at the `@implements` addresses.

##  SETTLED BY A FULL MATRIX RUN, 2026-09-03 (59ac8ed)

Scored VC4.2 / VC5 RTM / VC5 SP3 / VC6 against a **CONTROL SET of 61 known-byte-exact functions**: VC4.2 **22/61** (1,574 diff B), **VC5 RTM `11.00.7022` 61/61 / 0 diff B**, **VC5 SP3 61/61 / 0 diff B**, VC6 `12.00.8168` **45/61** (919 diff B). VC5 confirmed; **the two VC5 builds are indistinguishable** - SP3 has a genuinely different codegen (C2.EXE 630,544→660,240 B) yet is byte-for-byte identical on 0x100250D0 and 0x1000A110 entire, and WORSE on 0x1000EAF0 (30→37 regions). **The giants' residue is source or unreachable, NOT the toolchain - never reach for the patch level again** (3 idiom notes + project rule 11a corrected).  **THE LOST-SYNC TRAP BITES HERE HARDEST: VC4.2 reports 1 REGION on 0x100250D0 - the best-looking number in the project - and 100.0% of the function was NEVER COMPARED** (352 insns short, no re-anchor). A WRONG compiler gives the prettiest region count; always read the NEVER COMPARED line and always score on a control set, never on a hard function.  `tools/match_sweep.py` takes `BR_MSVC=<dir>`; stage alternates in a PARALLEL dir, never overwrite `tools/msvc5`.  **`reference/msvc/` ALREADY HOLDS VC4.0/4.1/4.2, VC5, vs97sp3 and vs6 media - look there before sourcing anything.**  Score with `match_sweep.score(orig, code, set(relocs))` + `load_orig(path, va)`; a raw byte compare reads 0/19 on a 19/19 file.
