# Brffb cluster

*Recorded 2026-09-12.*

> 2026-09-12: the BrFfb fresh-pool cluster worked end to end - 3 of 4 rows now size- AND insn-exact with Gate A passing (UpdateSpring 391/391, Init 434/434, EnumDevice 239/239), each one @t4-pass ledgered; Setup's A3 FAIL is the bare-decimal addend artifact, not code. Five new proven levers, incl. two brand-new structural ones (arg-slot reuse, else-at-tail).

**2026-09-12, the [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md) supply #3 executed: the
BrFfb cluster went from 4 diff rows to 3 rows at Gate 0+A PASS (each
size-exact, insn-exact, REGNORM 0+0 or 1-register residue) + 1 known-parked.**
Commits 998e202, b31a624, 23ba781, 062c6f7. Each has @t4-pass 1/2 in
slice3_45.c; Gate B still needs a second (census) pass per row.

**New levers proven this session (VC5), in reuse order:**

1. **`static __inline` on tiny arithmetic helpers** - plain `static` wrap
   helpers (mul/add/div) emit out-of-line calls; __inline materialises the
   magic-multiply /10000 inline. One edit took UpdateSpring 207 -> 47 diff
   bytes. Same class as the vtable-cast helpers ([com-vtable-levers-2026-09-10](../cpp-lane/com-vtable-levers-2026-09-10.md)).
2. **A named local for a CSE'd global web** (`k = g_br0BD424`, re-assigned
   after calls like the original re-reads) collapses a whole-function esi/ecx
   rotation AND a scheduling slip in one probe - but WATCH the side effect: it
   freed the allocator to FOLD a single-use global load into `imul r,[mem]`
   where the original keeps `mov r,[mem]; imul r,r`. TEN probes could not get
   both; parked at REGNORM 0+0 WITHOUT the k-local (the base multiset was
   already clean).
3. **`ret` local with end-of-body reassignment** (`ret = g; if (ret != 0)
   {...; ret = g;} return ret;`) - makes both early-guard exits jump PAST the
   tail reload (eax stays live), forces the count RMW into ecx (+2 B: 8B0D/890D
   vs A1/A3 forms). BrFfbInit's last 2 bytes.
4. **NEW STRUCTURAL CLASS - spent-argument-slot reuse:** BrFfbEnumDevice's
   original passes `&pDevInst` (the incoming ARG SLOT) as CreateDevice's out
   pointer - no local, no NULL init, frame = guid only. Spell it as
   `(BrDiObj **)&pDevInst` on a non-const param. Screen: an out-param lea
   pointing ABOVE the frame at [esp+argN].
5. **NEW STRUCTURAL CLASS - error block at the tail is an ELSE, not a goto:**
   VC5 pulls a single-ref goto target back inline (inverting jl to a short
   jge, -4 B). Nesting the entire rest in the success arm and letting the
   error text be the else lands it at the function tail exactly like the
   original. A `goto`+shared-label spelling can NOT reproduce it.
6. **Debug-print import spelling varies PER FUNCTION in one TU:** BrFfbInit
   caches `&OutputDebugStringA` (dllimport, `mov edi,[__imp__]; call edi`,
   stdcall) in a local; BrFfbEnumDevice calls the import DIRECTLY per site
   (`call [__imp__]`). Matching arm: `__declspec(dllimport) __stdcall` decl +
   BR_DBG_SINK / BR_DBG_OUT macros in slice3_45.c; port arm keeps BrDbgPrint.
   Also: dropping a named `hr` for direct `if (call < 0)` turns
   `cmp eax,esi` into the original's `test eax,eax` (4 sites).
7. **pVtbl hoisted as a named local before a struct fill** makes VC5 push the
   call args FIRST and sink every store past them (offsets +0xC), matching
   the original's schedule (BrFfbInit SetProperty, 42 -> 14). Residue: the
   original creates the &d temp before the vtable temp (ecx/edx swap, 3
   insns) - pHdr hoist folds away, call-embedded assignment loses the sink,
   decl order inert. Same creation-order class left on EnumDevice's
   SetDataFormat vtable temp (2 insns). Corpus MISS on both.

** BrFfbSetup 0x10072680 stays parked and its A3 FAIL is FALSE:** the 4
"unpaired" rows are `g_brFfb+4` / `g_brFfb+8` DIR32+addend relocs the masker
renders as bare `push 4` / `push 8` against the original's absolute addresses.
That is EXACTLY what the deferred "bare-decimal addend masking v2" stash fixes
([resume-state](../log/resume-state.md) STILL DEFERRED) - do not touch Setup before that migration
lands. The 6+6 REGNORM residue is the unmapped-globals bootstrap; relocmap.py
on slice3_45.obj: 146/230 relocs unresolved TU-wide - an image-fill
workstream, not a matching one.

**Bookkeeping traps re-confirmed:** report.csv rows go stale (EnumDevice
showed Od 946 B from a pre-__stdcall compile; the O2 truth was 256);
match_sweep's cache keys on file CONTENT, so `touch` does nothing - use
`--force`. A peer or the project lead committed my in-flight __inline edit with a correct
message mid-session (shared tree, [parallel-session-clobber](../traps/parallel-session-clobber.md));
include/br_carphys.h had a peer's uncommitted edit. fileaudit's 7 violations =
the stranded drift 18-vs-11 that PREDATES 09-10.

Related: [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md), [com-vtable-levers-2026-09-10](../cpp-lane/com-vtable-levers-2026-09-10.md),
[callee-saved-zero-web-class](../levers/callee-saved-zero-web-class.md), [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md),
[guard-shape-decides-prologue](../levers/guard-shape-decides-prologue.md), [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md).

**SECOND SESSION 2026-09-12 (same day): all three CERTIFIED @t3 - tree-wide
99 -> 103 (the 4th is a peer's 0x10067710 BrCrRespWalk).** Pass 2 for each:
slotcensus.py orig-vs-recomp (all three identical slot-for-slot) + 10 fresh
counted probes each (30 total), every one inert or worse - the residues are
real allocation forks.  TRAP RE-CONFIRMED: an @t4-pass or @t3 line keyed by
the D3D twin VA is INVISIBLE to `t3.py --qualify <glideVA>` - Gate B said "no
counted lines yet" until the lines were rekeyed to the GLIDE address, even
though the @implements tag itself carries the d3d VA. Key ledgers and tags by
the address you qualify with. Also: VC5 is C89 - a mid-block declaration in a
probe variant is a compile error, and a failed compile is not a countable
probe; fix and re-run.
