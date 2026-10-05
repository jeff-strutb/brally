# M2 pc session

*Recorded 2026-10-05.*

> PC M2 session (2026-10-05) T3->T4 levers: read a stored value back through its global; posweep AFTER a body change; parked rows with what was tried

PC/BRGlide M2 session (claims in build/match/m2_claims.csv). The project lead asked for PC, not N64, when saying "our T3 functions to T4 (M2)" on 2026-10-05.

Promoted:
- BrFramePresent 0x10023B70 (d6bb9e7f): `samples[g_BrFpsGateB] = delta` (index read back from the GLOBAL just stored) instead of `samples[gate]` made VC5 store gate's own zeroed register (`mov [g], ecx`) instead of the shared zero register (esi). Fix for the "constant zero propagated into the store" class.
- BrGbiTexScanLoadBlock 0x10029510 (adc220ec): `G = expr; len = G;` (value formed straight into the global and read back) fixed the memcpy remainder register; the last eax/ecx/edx pair only flipped with the function MOVED after `static br16_combine` AND a prototype in the file's declaration block. posweep on the tree body was inert; it only hit after the body change. **Re-run posweep (scratch posweep.py VA FILE) after every body improvement, not just on the tree body.**

- BrDlsTileSizeDecode 0x1001EC30 (0b65ccc6): esi/edi p-vs-ult fork (~140 spellings dead) fixed by giving ult NO local: `G = expr; if (G >= K) G -= K2; ... (lrt - G + 4)`. Found by scratch gscreen.py (read-back screen: env ELIMONLY=1 drops the local, MEMBER=1 does p->f targets). Screen ran over all C T3 rows: only this hit.

- ** N64 COUNTERPART AS SOURCE MODEL** BrPanelDlBuild 0x10010FB0 (7c371ffa): PC body was Ghidra-shaped IVs (rn 6+8, 100+ probes dead). It is N64 BrSkidDraw (src/tgrally/drawing/particles.c). Re-transcribed with N64 loop structure (i/g/v loops, computed pointers) -> rn 1+2; then `hi = (v+1)<<16` before the test (IV creation order = first mention -> register), and `for (g = 0, strip = car + 0x11a0; g < 4; g++, strip += 0x480)` (slot order) -> EXACT. For any PC T3 row: look for the N64 twin (same name or same constant/shape) and re-shape the PC body after it.

- BrCollRespTipKick 0x10066D70 (705092ad): N64 twin shape (count++ before pW; p written whole in both tail arms -> p/w slot order; plain m00*nx+m01*ny+m02*nz) + PLACEMENT. Method that worked: pad diagnostic (N `int br_pad_i(int x){return x+i;}` before the fn; >=56 made it exact = TU state) -> posweep/posweep2 (PW_EARLY) for a real slot -> check EVERY matched fn in the file at that slot (BrCrExact regressed; fixed by also moving BrCrExact early; a leftover `static` prototype at BrCrExact's old spot is load-bearing).
- BrMat3Solve: same TU-state signature (N64 body under /O2 -> 417 size-exact at >=59 pads or with the real maths TU 0x1006D2E0..DDD0 bodies before it); not landed (needs maths TU refile).

Lever generalisation: "read back through the global" changes which register VC5 stores/keeps (BrGlNavPoll got the xor/store pattern this way but tc/f eax/ecx stayed swapped, +2 B).

Session end 2026-10-05: PC T4 1387 -> 1392 (5 promoted), T3 108 -> 103. Candidates saved in build/match/m2_candidates: BrCarDamageTick (co-filed after BrSelLookup = 5 diffs, TU state), BrMat3Solve (N64 body + maths TU = size-exact rn 2+2), BrGhostPlaybackStep (N64 T4 twin's in-place path fraction fixes fdivr/fstp st(0); rn 1+1, index load order left), BrRaceCueLayout d14, BrUiSprBlit d65.
- BrGhostPlaybackStep lever came from the N64 session's same-day T4 (baa23b5b): re-check N64 T4 twins as the N64 lane promotes rows.
- C++ rows: score with the file's own variant (/O2 /Gi rows must run JOBS=1 serial); a /O2-vs-/Gi mismatch faked a pad "gain" on BrCarStartInit.

- Twin finder: scratch twin2.py matches each PC T3 row's distinctive immediates against each N64 function body (Jaccard). Found BrScenePropsDraw = N64 BrModelDraw (T4) and confirmed BrPanelDlBuild = BrSkidDraw; BrScenePropsDraw's pList-reload register stayed put under the twin's shapes.
- Single-temp-register residues (BrUiText1003F760 table load ecx vs eax; BrItemSetTotal/BrMenuTime0D70 siblings) are NOT TU state: predecessor pads of six kinds were inert.

Parked (dated 2026-10-05, tried): BrRaceBeginResetOnce 3 diffs (memset [4] store after loop init; 360 orders/forms), BrDlsTileSizeDecode (esi/edi; ~140 forms, co-filing into br_dl/br_dlcmd), BrCarStateLerp (fmul [t] issue order; opts /G4 /G5 /Op, inline lerps), BrGlNavPoll (see above).

Tools: scratch scoreall.py (scores every T3 row via fn.py), vrun.py VA files..., posweep.py / posweep2.py (PW_EARLY=1 earlier positions; fails when decls come later), sbs on build/match/obj_fn_<tag>/fn_<VA>_<tag>.obj (obj_fn_<tag> dirs are shared across VAs; glob with the VA).

Related: [m2-session-2026-10-04](m2-session-2026-10-04.md), [t3-to-t4-levers-2026-10-04](../levers/t3-to-t4-levers-2026-10-04.md).
