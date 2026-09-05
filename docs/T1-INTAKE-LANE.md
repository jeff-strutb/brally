# T1 intake lane (paste-ready spec)

Feed this file to a fresh session. It is the procedure that produced nine
byte-exact functions in one lane on 2026-09-05, written so nobody re-derives
it. It complements `docs/STRUCTURAL-PLAYBOOK.md` (which stays the rule book)
and does not replace `CLAUDE.md`.

## What the lane is

The tagged small-gap pool is worked out and parked with honest notes. The
count moves on **T1 intake**: functions that exist only as a machine draft in
`build/ghidra_decomp/<va>.c` (or `build/ghidra_work/<va>.refined.c`). Take
them **smallest first**. A 100-250 byte leaf goes byte-exact in 4-6 probes;
the first compile is already size-exact more often than not.

## Session start (in order, every tool through `.venv/bin/python`)

    tools/refcheck.py            # must say Glide-keyed
    tools/install_hooks.py       # idempotent
    tools/tiers.py               # the denominator; note T1/T4
    tools/claimcheck.py          # six FLAGGED d3d delegators are known; 0 duplicates is the bar
    tools/fileaudit.py           # note the baselines (11 / 58 / 0)
    git status --short src/      # another session's uncommitted files: do not touch those
    tools/tiers.py --list T1     # sorted by size DESCENDING; read it from the bottom

## Screening a T1 row (two minutes each, batch ten at a time)

For each candidate VA, one shell loop:

- `ls src/core/cpp/<VA>.cpp` and `grep <VA> build/match/report_cpp.csv` -- if
  either hits, the C++ lane owns it. Skip.
- `grep -ic <VA> build/match/lane_claims.csv` -- claimed or parked. Skip.
- `xxd -l 6 -p build/match/orig/<VA>.bin` -- `6aff`...`fs:[0]` prologue is a
  C++ EH frame; an odd address is a merged/split map row. Skip both.
- `tools/dumpasm.py <VA>` and the draft, side by side. Reject on sight:
  x87 stack juggling (`fxch` chains), 16-bit register arithmetic (`movsx cx`,
  `test cx,cx`), byte-lane packing with `mov ah/dh`. Those are byte-slot /
  colouring walls and the notes say so already.

Keep: plain control flow, calls to already-matched neighbours, integer
arithmetic, struct field traffic, one or two float compares.

## The loop for one function

1. Find the module. `grep -i "^0x<prefix>" config/filing.csv` for the
   neighbours; the port twin (grep the VA in `src/` and `include/`) often
   names the module and already has the field names. A new `br_*.c` module
   file is fine; a new `sliceN_MM.c` or a new tag in an old one is refused
   by the hook.
2. Transcribe from the draft into that file, matching-arm only
   (`#ifdef BR_MATCHING_BUILD`), declaring callees and globals locally with
   the names already in `config/globals_learned.csv` (a new name is learned
   on the first match; two names for one address is normal). Write the
   `WHAT IT DOES:` comment and the `@implements <VA> glide <Name>` tag in the
   same edit -- the hook refuses one without the other.
3. `tools/match_sweep.py <file.c>` (~12 s). Read EVERY row of that file in
   `build/match/report.csv`, not the totals.
4. If diff: `tools/fnmatch/fn.py <VA> --detail regnorm 30` for the shape
   summary, then **always** the side-by-side:

       tools/sbs.py build/match/obj_<opt>/<file>.obj <Name> <VA>

   `<opt>` is the report row's column (O2/Od/O2y/O2p); fn.py rewrites
   `obj_O2`. The flagged rows tell you WHERE. The summary misled twice in
   one lane; the dump never did.
5. Probe with `fn.py <VA> --make <tag>` then edit
   `build/match/t3d/fn_<VA>_<tag>.c` and `fn.py <VA> --var <tag>`. ~10 s a
   probe. Generate several variants from one Python heredoc and score them in
   one loop. **Budget: six probes.** Past that, write the residue note
   (what diverges, every dead probe) into the function's comment, commit it
   as T2, move on.
6. Byte-exact: one-file sweep again, then
   `git commit -m "<VA> <Name> byte-exact: <the source fact>" -- <file>`.
   Never a bare commit; another session commits with pathspecs and takes a
   shared file's whole working state.
7. Any newly proven construct goes to the TAIL of `docs/VC5-IDIOMS.md` in
   the same sitting, with the dead probes listed.

## Levers that closed functions in this lane (details in VC5-IDIOMS.md tail)

- A loop-carried value written as an EXPRESSION of the counter becomes an
  induction temp and takes the INDEX register; a named local bumped each
  pass takes the BASE. Two SIB bytes, and no source order or declaration
  order moves them.
- A dead-after-test result local (`wr = Wait(...); if (wr == 0)`) swaps
  which of two hoisted import pointers gets ebx. Test the call inline.
- A parameter used as the cursor is loaded in the loop preheader; a
  `p = param` local is loaded above the pushes.
- Block layout follows the arms: an arm written as the ELSE of `if (c !=
  EOF)` is laid out last and keeps its own tail; a `while (n < cbMax)` keeps
  the count exit's own epilogue where `for(;;)` + inner return folds it.
- /Od homes locals in declaration order, bottom of frame up.
- A 12-byte vector copied into a record is three scalar float copies through
  GP registers; a struct assignment forms the destination address first.
- A pointer local initialised at its declaration is hoisted above the first
  branch; assign it where the original computes it.
- In a 4-byte swap the byte you SAVE is the one that gets eax.
- Under /Od-style codegen `(int)float` is a `__ftol` call; `q = n / 100;
  n -= q * 100` gives the magic-multiply divide (already in the file).

## Bookkeeping that bit

- `tools/filing.py` with no arguments rewrites `config/filing.csv` from
  `report.csv` and DROPS any row whose VA is not in report.csv -- another
  session's in-flight functions. Diff before committing:

      git diff -- config/filing.csv | awk '/^-0x/{split($0,a,",");d[substr(a[1],2)]++} /^\+0x/{split($0,a,",");p[substr(a[1],2)]++} END{for(k in d) if(!(k in p)) print "LOST", k; for(k in p) if(!(k in d)) print "NEW", k}'

  Re-add LOST rows by hand (the file is CRLF; write with `newline=''`).
- The other session's pathspec commit can swallow your filing.csv rewrite.
  Harmless, but `git status` will surprise you.
- A header prototype that conflicts with the byte-exact signature (the
  port's cdecl twin vs the original's thiscall, or FILE* vs the CHK handle):
  `#define Name Name_port` before the include, `#undef` before the
  definition. Precedent: br_bitstream.c, br_appstart.c, br_camframe.c.
- MSVC5's `<stdio.h>` defines `getc` as a macro; the original calls the
  function. `#undef getc` after the include.
- A function in the /Od stretch (0x1002A840-0x1002BF50) needs its own TU or
  an /Od neighbour; the sweep picks the variant per function and mixed
  variants in one TU are accepted by the gate.

## End of session

    tools/filing.py            # then the LOST check above, then commit filing.csv
    tools/image_build.py       # must say IMAGE GATE PASSED, 0 differing bytes, all four binaries
    tools/fileaudit.py         # baselines unchanged
    tools/claim_lane.py release <TOKEN>

Report the byte-exact count with its denominator (tiers.py T4 of the hand-C
target; total.py for all lanes), list matched and parked separately, and say
which strictness each claim is.

## Where the lane stands (2026-09-05)

Done: 0x100023F0 0x10035BE0 0x10032530 0x1006AAF0 0x1002B3F0 0x10003530
0x100154A0 0x10002310 0x10031960. Parked T2 in `src/core/audio/br_sndload.c`:
0x100701B0 (8 B, block layout), 0x10070280 (43 B, two copies' registers).

Next smallest unscreened T1 rows, with the reason each was deferred:
0x10023CB0 (112 B, RGBA5551 packer, 16-bit lanes), 0x10019040 (182 B, DL
patch, big-endian byte lanes -- the Horner idiom is in VC5-IDIOMS.md),
0x100541B0 (196 B, thiscall glyph walker, 16-bit chars), 0x10024680 (205 B,
x87 fxch chain), 0x10002460 (252 B, rep stosd/movsd struct copies -- clean),
0x1005F580 (259 B, standings qsort -- port exists in br_racestep), 0x100684F0
(265 B, suspension spring floats), 0x100704E0 (272 B, DirectInput vcalls).
Start with 0x10002460 and 0x1005F580.
