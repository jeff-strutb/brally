# Brtex3dexpand t4 handtranscription

*Recorded 2026-10-05.*

> 0x100250D0 BrTex3dExpand T3->T4 hand-transcription run (session, 2026-10-05): 763 -> 399 differing insn lines; the levers that moved it, the D3D twin, and where it is stuck (I4 blend bodies 2/3)

The project lead asked (2026-10-05) for the largest PC T3 rows to be hand-transcribed to T4; this session took BrTex3dExpand (claimed in lane_claims.csv; BrRaceStep and BrSceneDlBuild went to peers).

Metric used: aligned diff lines (scratch al.py: difflib on register-blind shapes, jumps ignored) = structural ! + register ~ + slot-only `.`. Start (saved candidate build/match/m2_candidates/0x100250D0_*_frame68_struct223.c) 800; best 399 (frame 0x68, regnorm 22+27, 2402/2407 insns). Best source kept in the session scratch as best_407.c / climb result; NOT committed (tree still has the certified T3 body).

**Levers that moved it (each measured):**
- Use param_1 / param_2 directly as output cursor and budget (no puVar21/cbMax copies): prologue load order matches.
- Mirror-block (mirrorT) read pointer is the source-row pointer variable (param_4), as the original's `[esp+0x88]` stores show.
- **CI4raw: drop the `cols` local** (`param_9 = 1 << (maskS-1)` directly) -> head + CI4raw + CI4pal become instruction-identical (635 -> 559). The zero-register web at the loop head vanished with it.
- 8-bit arms: `*out = v; out++; [q+-1;] cb++; if (cb >= max) return;` (Ghidra had cb++ first).
- IA4 even path: Ghidra's `param_9` counter is fake; a plain local counter fixed IA4/I8/RGBA (527 -> 433).
- For-init counters (`for (x = 0; x < w;)` in BOTH odd/even paths, no shared `x = 0` before the mask test) restore the original's `cmp ebx,eax` zero-register guard (IA8 blend, I4 blend).
- Per-arm block-scoped scratch locals (scope itself is inert for packing; it decouples symbols) and block-local byte channel vars per blend body.

**Facts established:** VC5 gives ONE frame home per symbol (listing `_x$ = off`); stack packing uses true liveness (block scope inert); decl order is inert for packing but moves register tie-breaks; variable names inert; /Fa listing (`cl /Fa`) shows symbol->slot, use it. c2 `/d2db#` accepted but prints nothing (retail).

**D3D twin:** BRD3D.dll 0x10025AB0 (8288 B) is the same source with 32-bit texels (add esi,4 / ARGB8888 byte adds). Its loop head allocation equals the Glide original's, so head residue is source-determined. It does not pack into dead param slots (frame 0x8c).

**Where it is stuck:** I4 blend bodies 2/3 (~200 of the remaining lines). Original: source byte stays in bl, intensity byte is homed (byte store + dword reload & 0xff, slots 0x4c/0x50), r homed at 0x13. Ours: source byte homed, merge done in dword regs. Every text variant tried (inline widening, named/inline deltas per channel x16, block/arm byte vars, order) either leaves it or flips the body to body-1 codegen. Cleaning CI8's `iVar16 = ctr+1; ctr = iVar16` (CI8 50 -> 22) makes cbMax (param_2) fully enregistered and wrecks I4 blend/IA8 blend (global coupling through cbMax's spill region) - fix bodies 2/3 first, then redo CI8.

Related: [brtex3dexpand-t3-2026-09-16](brtex3dexpand-t3-2026-09-16.md), [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md), [register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md).
