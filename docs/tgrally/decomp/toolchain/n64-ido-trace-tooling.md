# N64 ido trace tooling

*Recorded 2026-10-04.*

> Instrumented IDO 5.3 (uopt allocator trace + register forcing) built 2026-10-04; n64alloc.py trace/force/sweep/diagnose; how to read forces and declines

Built 2026-10-04 at the project lead's direction ("build the optimizer and any other tooling we need"); project lead approved cloning decompals/ido-static-recomp and akratch/n64-decomp-workbench into build/ext (gitignored, never committed).

- `sh n64/tools/build_ido_trace.sh` rebuilds build/ext/instr/out/cc: IDO 5.3 recompiled at ido-static-recomp 9c242adc (uopt.c sha b0058f15, the workbench pin), workbench uopt globalcolor+alias and ugen freelist+emit profiles, plus an ecvt/fcvt libc shim (n64/tools/patches/) so `-Wo,-zdbug:2` (writes ./uoptlist itable + flow graph) doesn't abort. Identity gate: tracing off, 176/176 N64 sources compile identically to tools/ido53.
- `n64/tools/n64alloc.py trace|force|sweep|diagnose VA [--file draft.c]`. Force keys `p1:wN=cK` / `p1:wN=s` (split); colours c1-5 v0 v1 a0 a1 a2, c6 a3, c7-12 t0-t5, c14-22 s0-s8. Sweep uses an ALIGNED diff (positional T4 count is useless once lengths differ); `--greedy N` stacks forces.
- `diagnose` runs the workbench on ROM/candidate dumps: separates uopt's colour pool from ugen's temp ring and names the owning pass.
- A `force_declined ... forbidden=` record means an interfering neighbour holds that colour; use CDX_DETAIL_WEB=N to list `intf` neighbours (assigned=K) and force the blocker too.
- Web -> C variable: webdetail exprtable/exprchain index the itable in uoptlist (e.g. `[8329,1] isvar M 3 -116` = a frame local at vfp-116).
- First findings: BrPaintShopScreen's biggest lever is splitting r's live range (web 160, 779 -> 732 aligned; greedy reaches 474). BrSkidAge's 3-word residue is a uopt v0/a0 colouring tie (web 459 blocked by web 241 holding v0).

- Batch 2026-10-04: greedy force search over 41 T3 rows (ndiff<=120) -> 6 EXACT under forces alone (BrBootCheck w74=s, BrSchedInit w89=s, BrCarDrawWheels w61=c21+w59=c20, BrSeasonRaceDone w58=c4, BrMusicInit w99=s, BrPathWalk w50). The other 35 need source/structure, not just colouring.
- LEVER (BrMusicInit -> T4, 1c06052f): a constant ours holds in a register while the ROM rematerialises it can be split by giving one use a different type (extern unsigned vs int): IDO doesn't merge int and unsigned constants into one web.
- The CDX `table=` of a constant web is its value; webs tied on `save` are taken in web order, which follows block order.
- Scratch `n64build.py --diff` calls MUST pass `--csv /dev/null`, or they write scratch rows into the shared build/n64/verify.csv.

- Open cases, measured 2026-10-04:
  - BrCarDrawWheels: i/IV net 31 vs constants 30; the ROM ranks them below.
  - BrBootCheck: the address web nets 1 (the arg occurrence isn't nl-charged).
  - BrSchedInit: the 1/4 constant tie.
  - BrEntRebaseModel: +8 frame = cfe locals area. Counters as parameters fix the size but move the spill to 0x40 (ROM 0x44).
  - BrU16QueuePop: the ugen trace shows OR->t9, srl->t0 (ROM order), but the object has them swapped, so the swap is after ugen's temp allocation.

Related: [n64-m1-complete-2026-10-04](../log/n64-m1-complete-2026-10-04.md).
