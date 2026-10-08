# N64 racegatestep

*Recorded 2026-10-06.*

> BrRaceGateStep 0x8022A0E0 (rank.c) T4 2026-10-06 d9eb16c6, gate 680/0: int-returning osSyncPrintf prototype forbids v0 via localcolor; goto-into-join vs if-block decides PRE hoist; uopt forbidden = per-block regsused masks

**DONE 2026-10-06, commit d9eb16c6 (session eba7ae), image gate 680/0.** car.h BrCar.msgA/msgB became `char *` (racehud.c, racetick.c casts dropped).

**Levers that generalise (N64 IDO 5.3):**
- **Prototype return type matters:** `int osSyncPrintf(...)` (the real one is void). The register var for a used call result (`lod reg2`, e.g. `if (!BrSegmentsOverlapXY(..))`) gets a localcolor live range covering every later call block until another used result; localcolor ORs v0 into those blocks' regsused mask, so any web living there gets v0 forbidden. With `void` printf the CSE webs took v0/v1 as in the ROM. If a web is v0-forbidden with no v0 neighbour, check prototypes of calls in its blocks.
- **uopt forbidden0** = OR of per-block regsused masks (bb+44+class*8) over the web's blocks (f_updateforbidden), seeded by localcolor (f_localcolor marks the return reg across the result var's blocks) and by every web coloured earlier. intf lists do NOT show these.
- **`if (x != 0) goto label;` vs `if (x == 0) { ... }`**: same CFG, but the goto form let PRE hoist a load at the label (partially available from a branch inside the skipped block) into both predecessors (v0 web + edge-split `b`). The block form leaves the load in place. Probe in scratch pr/s1.c vs s5.c.
- Statement order of constant stores changes as1 scheduling of hoisted la/li (`msgATime = 1.5f` before `msgB = "..."` as the PC twin has it).

**Tooling:** scratch instr8/uopt = instr7 + `CDX_UF=1` logs ([UF] who adds v0 to a web's forbidden, [LC] localcolor block marks). Built from build/tgrally/ext/ido-static-recomp with the build_ido_trace.sh flags in ~4 s. Diagnostic cuts of the body (hd.sh/hz.sh) bisect which construct causes a colour or hoist.

Related: [n64-carddriveinput-wip-2026-10-06](n64-carddriveinput-wip-2026-10-06.md), [n64-ido-trace-tooling](../toolchain/n64-ido-trace-tooling.md), [n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md).
