# Scenedl natural retranscription

*Recorded 2026-10-05.*

> BrSceneDlBuild 0x1000EAF0 T3->T4 hand transcription (session, 2026-10-05): N64 twin = BrTrackDraw; Ghidra body's IVs/temps were compiler-made; measured levers and current best variant

Session (2026-10-05) took 0x1000EAF0 BrSceneDlBuild (claim in build/match/lane_claims.csv) on the project lead's "hand-transcribe the largest PC T3s to T4" instruction. Peers: had BrRaceStep 0x10019A70,  had BrTex3dExpand 0x100250D0.

**N64 twin:** src/tgrally/drawing/trackdraw.c BrTrackDraw (0x80235BAC) is the object pass (split/last deferral, `last - i + split` reverse read, `end` fixup). The PC trail section has no N64 twin.

**Ghidra body was transcribing VC5's own temps** (32 passes of spelling probes on it): `pCar`/`negCar0` (= 0xffffd620 - param_4) are VC5 strength-reduction of `param_4 + iCar*0x2b68`; `nTotal`/`pDst`/`firstVis` are VC5 IVs from `list[++cHead]`, `list[cHead - i + base]`, `DAT_102e0ca0 = cHead + 1`. Writing the natural source makes VC5 mint the identical temps.

Measured wins (aligned structural cost 85 -> 46; full regs+slots 239 -> 161):
- drain loop: `for (dw..; dw++, row += 500, dring += 4)` + nested if/else with TWO `pA[dw] = 0` stores (VC5 cross-jumps them into the original's single block). The dossier's "nested form dead" verdict was dated.
- object loop as N64 `for (; i < n; i++)` with `continue`s: else-arm reloads cHead/base before the join exactly like the original ("wall 5" gone).
- `base` declared BEFORE `cHead` (pair order is a real tie-break; nothing else in the decl list moves).
- wheel block: `pW = WR; pP = &WR->x; dx = pW->vx; ...` with WR = (BrWheelRec*)(param_4 + iCar*0x2b68 + iw*0x40).
- copy loop: plain `for (k = 0; k < 4; k++)` reproduces the count-down loop.
- trail else-arm: no `head` local; read DAT_1035faf0[r] directly, `next` local, test `TAILS != HEADS` (tails first).
- Row-pointer decl order must be re-swept after ANY local add/remove (symbol-index x87 tie-break); best class e.g. VPWZY.

Dead this session: TU co-filing with real predecessors (predecessors not exact; output identical to standalone), synthetic pad states (baseline best), ternary TPREV/TNEXT macros (+11 insns), typed car param, 2-D heads/tails, unsigned ring types.

Tools in session scratch (lost at session end): adiff.py (difflib-aligned diff, reloc-patched, --raw/--slots), rowperm.py, declsw.py. Best variant file: build/match/t3d/fn_0x1000EAF0_k1.c (gitignored build dir).

Open: in-loop constant regs (orig esi=0xfffa), row notch 0xd9a, wheel addr term order 0x1be7, ring*4 CSE (wall 4) + pDst homes, else-arm mov/dec + inc-vs-lea, drain shl/arena order, tail fixup, slot map (i<->IV swap 0x1c/0x20).

Related: [hand-transcription-only](../rules/hand-transcription-only.md), [m2-pc-session-2026-10-05](../log/m2-pc-session-2026-10-05.md), [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md).

**Stopped 2026-10-05 (project lead call), still T3.** Best variants kept in build/scenedl-keep/ (z1 = natural body, 44 structural hunks plain; w60 = z1 + 65060 leading externs, 29 hunks / full 146; ghb = Windows+CRT+Glide prefix). Floor: B7 OR order flips whenever the late locals wrap past 65536, which the arena/tail/wheel walls need; ring*4 (wall 4) and const-6 placement never closed. Claims released. Nothing changed in src/.
