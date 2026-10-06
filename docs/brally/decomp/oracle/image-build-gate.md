# Image build gate

*Recorded 2026-09-04.*

> tools/brally/image_build.py assembles all matched functions into the real binaries and diffs vs original - the deliverable-level gate the per-function sweep cannot be. Since 2026-09-03 it builds ALL FOUR in-scope binaries into build/brally/win32/image/ and EXITS 1 on any bad claim; reference-filled slots are not evidence.

## 2026-09-03 (later): all four binaries, and the gate now EXITS 1

`python3 tools/brally/image_build.py` builds **BRGlide.dll + BRally.exe +
SetVideo.exe + BossRally.exe** into `build/brally/win32/image/` (drop-in set; BRD3D.dll
copied verbatim, out of scope). All four green: 0 differing bytes each  - 
BRGlide 1,022 fns / 163,492 B / 34.00% of .text, BRally 28 / 79.80%,
SetVideo 41 / 14.00%, BossRally 35 / 10.54%. `--no-write` gates only,
`--only <name>`, `--out-dir`, `--recompile`; legacy `--out` still writes the
DLL to one path.

**It is a hard gate now: exit 1** on any differing byte, overlapping claim,
two names at one address, function outside .text, **or a claimed match the
tool could not build and place**. That last one closed the only way it could
lie - an unbuildable claim used to drop out silently while the run still
printed "0 differing bytes" over less work than the report claims. A failed
image is written as `<name>.FAILED`, never as a drop-in.

**EXE lane specifics.** exe_sweep scores through a temp dir it deletes, so
there is no obj to reuse: all 104 EXE matches are recompiled here (cached by
mtime, ~40s cold / seconds warm). Their relocs resolve through the EXE's OWN
matched functions ONLY - `load_maps` and `globals_learned.csv` are
Glide-keyed, and letting an EXE reloc through them writes a 0x10xxxxxx
address into a 0x004xxxxx image on nothing but a shared name (hence
`compiled_functions(learned=False)`). Everything else takes the reference
dword. Also: the EXE report spells CRT-shaped names `_matherr` /
`_setdefaultprecision` while the COFF undecorator strips the underscore  - 
key the EXE fnmap BOTH ways or those rows read as "symbol not in obj".

**A green emit is byte-identical to the original** (unclaimed bytes come from
it), so "it runs" is not evidence our C executes correctly - the file proves
PLACEMENT. Don't let the emitted binaries be quoted as a runtime result.

## 2026-09-03: it was blind to the C++ lane - fixed, and reference-fill is now split out

**A lane not read by the gate is a lane nothing checks for collisions.**
image_build read only `report.csv`, so the 149 four-piece C++ matches
(`report_cpp.csv`, 40,182 B, in this same DLL) had been scored one at a time and
never laid down beside the C claims. The gate now places both:
**971 functions (822 C + 149 C++), 118,052 B, 24.55% of .text, 0 differing
bytes, 0 overlapping claims** - the two lanes do not collide.

Placing a C++ claim needs the **raw COFF symbol cpp_sweep actually scored**, not
the `@cpp_symbol` tag: a cdecl tag is written without its leading underscore,
class-name tags resolve through `cpp_score.find_symbol(prefer=kind)`, and
`parse_coff_code`'s keys are a MIX of raw and undecorated. `_raw_symbol` asks
cpp_score which symbol was scored and maps that back to the raw name;
`compiled_functions(only=...)` takes addresses from report_cpp.csv, since
mangled names are in no surveyed map. Objs are
`build/brally/win32/match/obj_cpp/<base>_sweep_<VA>_<i>.obj`, where `i` is the index into
`cpp_score.DEFAULT_OPTS` - the report records only the short opt tag, so map back
through `cpp_sweep._opt_tag`.

**Reference-filled slots are NOT evidence.** A reloc slot whose target has no
address in any surveyed map takes the reference image's own dword, so it cannot
fail the diff. The gate now prints this per lane: **C 1.1%, C++ 18.3%** of placed
bytes. Quoting the image byte total without that split overstates the C++ lane.
Resolving C++ mangled symbols to real addresses is open work.

**Trap when measuring any of this: `report.csv` / `report_cpp.csv` drift LIVE**
if another matching session is running - a count taken in one python process and
compared against one taken in another will disagree by a few rows and look like a
tool bug. Reconcile inside a single process.

## image_build.py is the TRUE clean-state gate - not match_sweep

`python3 tools/brally/image_build.py` lays every `status==match` function into a copy
of the original BRGlide.dll at its claimed address and diffs the whole image.
This catches what per-function scoring **structurally cannot**: overlapping
claims, two names at one address, wrong sizes, and - the one that bit us  - 
functions that match in isolation but are placed WRONG in the assembled image.

**As of 2026-08-27 (`git log` for the commit): `ASSEMBLED IMAGE vs ORIGINAL:
0 differing bytes - every claim holds at image level.`** 570 DLL-C functions,
45,330 B (9.43% of .text). Run it after any batch of matches; it is fast
(reads objs + report.csv, no compile) and is the honest answer to "are we in
a clean place" at the deliverable level.

## The preamble trap it exposed (fixed 2026-08-27)

Six thunks (0x1001E1E0/200/220/250/280/2B0) scored `match` in `report.csv` but
the image showed 188 differing bytes across them. Cause: they carry a 16-byte
**link-stage preamble** (`e9 0b000000` = jmp +0x0b, then 11 nops) recorded in
`config/brally/preambles.csv`. `match_sweep.load_orig` STRIPS+verifies that preamble
and matches the compiler's body (which starts at +0x10) - correct. But
`image_build.compiled_functions` was laying the body at offset 0, misaligning
the whole function.

**Fix (landed):** `image_build.py` now imports `PREAMBLES` from `match_sweep`,
reads `body_n = orig-plen` bytes, resolves REL_REL32 against the body's real
image address `va+plen+off` (not `va+off`), masks static slots from
`body_orig` (= `orig[plen:]`, NOT the full bin), and prepends the recorded
preamble verbatim. Preamble bytes are link output - same category as relocs.

**LESSON (durable): a per-function `match` is NOT proof the byte-exact
deliverable is clean.** The sweep answers "do these bytes equal the original
at this address"; a function can answer yes and still be placed wrong. Always
run image_build.py before claiming image-level cleanliness. See
[implements-requires-execution](../rules/implements-requires-execution.md), [counting-reconciliation](../traps/counting-reconciliation.md).
