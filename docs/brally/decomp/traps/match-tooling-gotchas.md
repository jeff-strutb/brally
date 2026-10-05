# Match tooling gotchas

*Recorded 2026-08-21.*

> Sweep scorer blind spots (statics, decoration), the --help footgun, dumpasm gaps, glide/d3d twin addressing, and the scratch-object trick.

**1. Symbol decoration. FIXED (2026-08-19).** `tools/match_diff.py` keyed COFF
symbols with `lstrip('_')`, which undecorates cdecl and nothing else - fastcall
is `@Name@N`, stdcall is `_Name@N`. Any non-cdecl function reported
`not_in_obj` and could never score. Replaced with an `undecorate` helper.

**2. Statics are invisible to the scorer.** `parse_coff_obj` only reads
external symbols (`sclass == 2`), so a `static` function can match
byte-for-byte and never be credited. `BrBitStreamAlignRead` does exactly this.
This is one source of drift between the sweep total and hand-verified counts.

**3. glide/d3d twins.** A function tagged `@implements <glide-va>` is scored by
the sweep at THAT address, which may not be the address you diffed by hand.
Some glide addresses are not in `config/functions.csv` at all, so the function
is unmeasurable either way (`BrExt_1007AC00`). ALWAYS diff the address the tag
names when reconciling counts; use `config/shared.csv` to find the twin.

**Consequence of 2+3: hand-verified counts run AHEAD of the sweep total.** The
sweep is the number to plan from; state both if they differ rather than picking
the flattering one.

**4. `match_sweep.py` treats any unknown `--flag` as a full sweep.** It filters
`--`-prefixed args, so `--help` leaves zero arguments and silently starts the
~20 minute run that rewrites `report.csv`. Three of five workers tripped it.
A killed run does no damage (the report is written only at the end) but it
truncates `.obj` files mid-write - re-run the single-file sweep afterwards.

**5. THE SCRATCH-OBJECT TRICK - use this, it removes the main blocker.** A full
sweep owns `build/match/obj_O2` and `obj_Od`, so single-file sweeps race it.
You do NOT have to wait: compile by hand to `build/match/obj_TEST/` and objdiff
that.

```
sh tools/wine.sh tools/msvc5/bin/cl.exe /nologo /O2 /W3 /I include \
  /I tools/msvc5-compat /I tools/msvc5/include /DBR_MATCHING_BUILD \
  /c src/core/FILE.c '/Fobuild\match\obj_TEST\FILE.obj'
.venv/bin/python tools/objdiff.py build/match/obj_TEST/FILE.obj _Name 0xVA
```

Note the `/Fo` backslashes and the repo-relative paths - cl.exe reads a leading
`/` as an option prefix and forward slashes make it write nothing.

**6. `dumpasm.py` has no boundary for many small leaf functions.** It answers
"no function at <va>" for perfectly valid addresses and can decode garbage if
forced. `objdiff.py` resolves the original bytes correctly regardless - prefer
it and treat a dumpasm miss as a tooling gap, not a wrong address.

**7. report.csv goes stale PER FILE, and the overall total hides it.** Counting
zero-diff rows does NOT prove the report is current - one file read as 22 diffs
while a fresh sweep of identical content said 11/24 matched. Full details and
the re-baseline procedure in [report-csv-per-file-staleness](report-csv-per-file-staleness.md).

Related: [matching-progress](../log/matching-progress.md), [divergence-class-triage](../triage/divergence-class-triage.md),
[report-csv-per-file-staleness](report-csv-per-file-staleness.md).

## Symbol decoration is a RECURRING blind spot (2026-08-23)
The scorer was fixed for `@Name@12` fastcall/stdcall symbols long ago, but
image_build.py AND reloc_learn.py each had their own private `lstrip('_')` and
silently dropped every fastcall/stdcall function (40 of 456 claims). Fixed in
both; if any tool ever matches COFF names again, use
`name.lstrip('_@').split('@')[0]`, and grep for stray `lstrip('_')` first.

## image_build is the reconciliation authority
Three strictness levels exist: scorer-match (report.csv), name-resolvable,
image-placed. `python3 tools/image_build.py` must show claims == placed and
0 differing bytes; per-file STATIC targets are reference-filled and counted on
their own line. Header-comment addresses are NOT a valid map source - proven
stale/D3D-space by a 1,094-byte image diff.
