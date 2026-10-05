# Bracestep hand transcription

*Recorded 2026-10-05.*

> BrRaceStep 0x10019A70 (11,223 B, largest PC fn) went T3 -> T4 by hand transcription (193bdbda, image gate 1394/0). Levers: /Gi chain idb, block scoping, empty-loop dead store, critical-edge split breaks VC5 constant webs (fix = real else block that coalesces away), shared-tail misnesting, TU neighbour for fadd order. Plus the C2 instrumentation kit.

**DONE 2026-10-05: BrRaceStep 0x10019A70 T4**, commit 193bdbda (+ treemap affad33d). cpp_score
MATCH 0 diffs 4/4 under the /Gi chain idb; image gate BRGlide 1394 placed, 0 bytes differ.
Claims released (m2_claims + lane token e7a4d751).

How it was measured: /O2 /Gi with a private copy of build/match/probe_bd4988/idb/chain.idb (the
preceding Gi rows compiled in (file,va) order). For report_cpp.csv, cpp_sweep -j 1 was run with the
repo-root vc50.idb temporarily replaced by chain.idb, then restored byte-for-byte.

Levers (all measured):
- Block scoping decides IV operand order and stack slots; separate variables for the best-search
  loops, waterfall counter, phase-4 flag. Empty loop `for (k = g_0B3858; k < g_0B2F04; k++) ;`
  explains the original's lone dead store of k.
- **Critical-edge split breaks constant webs (new VC5 class).** A loop whose failed-test edge is
  critical and needs a PRE reload (here g_5CCBA4) gets a late split block; the zero constant's web
  does not extend through it, so the loop gets its own zero (preheader xor) and the whole tail
  recolours. Fix = a REAL else block on that edge whose code vanishes later: put the call result in
  its own variable and copy it into `now` in BOTH arms (copies coalesce away). Empty else, dead
  stores, self-copies, ternaries, empty inline calls all vanish too early (no block). `else ok=0`
  keeps the block but leaves an xor.
- VC5 does NOT hoist a common leading call out of if/else arms; two-call forms only tail-merge.
- cpp_score masks relocs and jump displacements can hide misnesting: a `je` to a shared tail showed
  `if (g_226A48 && g_226A44) sub_10005400;` belongs after `if (g_5CCB80)`, not inside it.
  Always read cpp_score's byte diff even when the aligned diff is clean.
- fadd operand order needs the original TU neighbour that first reads g_6E9D8C (0x10019840, gated
  draw) ahead of BrRaceStep. It is claimed in br_drawgated.c (shared with D3D, port patches it), so
  an UNCLAIMED copy named BrRaceDrawGated sits in the .cpp. Claiming it there double-claims
  0x10019840 and fails the image gate. Original TU = 0x10019350..0x1001C647 (window proc, main loop,
  S17 static Clock object at 0x105BC858, race begin helpers); co-filing the rest was inert.

C2 instrumentation kit (build/match/probe_bd4988/c2cap, reusable for any VC5 colouring wall):
mingw-built c2wrap.exe used via `cl /B2Z:\...\c2wrap.exe`; env C2PATCH=1 runs C2P.EXE, a C2.EXE copy
with an added .hk section (hk/mkpatch.py, i686-w64-mingw32-as) that logs the colouring candidate
loop in FUN_0042ad28 (symbol kind byte sym+4: 3 temp, 4 local, 7 global, 0xd constant; P sort key
node+0xc; per-block priority updates 0x41ea86/0x41eab3). run.sh NAME logs to NAME.log. winedbg
breakpoints do not work here; binary patching does. Ghidra headless needs
JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home.

Related: bracestep-wall, [gi-serial-idb-a7-recipe](../oracle/gi-serial-idb-a7-recipe.md), [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md),
[hand-transcription-only](../rules/hand-transcription-only.md).
