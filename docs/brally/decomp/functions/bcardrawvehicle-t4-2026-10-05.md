# Bcardrawvehicle t4

*Recorded 2026-10-05.*

> BrCarDrawVehicle 0x1000A110 (7,577 B) T3 -> T4 in one session by reading the original's frame and symbol identities (d54647fd, image gate 1395/0). Levers: wrong D3D-name identities, frame map via /FAcs + depth tracker, byte array elements [1]/[2], per-arm statements + cross-jump, BrVec3 eye, typed model global for base/index, volatile for fadd, scheduler copy order.

DONE 2026-10-05: 0x1000A110 BrCarDrawVehicle T4, commit d54647fd (+ treemap 8ac856b6).
The 18-session dossier had called the residue a "byte-compose / pack wall". It fell in
one session to these, all found by reading the original rather than respelling:

- **Wrong symbol identities.** The colour arms used D3D-addressed `BrG_6Cxxxx` names that
  stood for DIFFERENT Glide addresses at different sites (the image gate rebound them via
  10 "scramble" rows in config/brally/reloc_overrides.csv). VC5 value-numbers by symbol, so lane
  order and load order were wrong. Naming each byte global by its Glide address
  (`DAT_106e8610` etc.) fixed the "unreachable" lane flip at once.  Any function with
  hand reloc_overrides rows: suspect wrong symbol identity first.
- **Frame map.** /FAcs equates (`_x$ = -N`, disp = frame+pushes+N) for ours; for the
  original, a CFG esp-depth tracker (scratch frameinv.py) turns `[esp+X]` into frame
  offsets and lists every use of each home. Name each home by its uses; holes are
  unwritten aggregate members (here BrVec3 eye.z). VC5 packs aggregates into bins
  first, then first-fits spilled scalars into bins (incl. DEAD ARG SLOTS), else
  sequential in spill (colouring) order. Declaration order is inert.
- **Byte array elements [1]/[2]** of `uint8_t pack[4]`: VC5 homes both and reads back
  widened (`mov eax,[esp+0x31]; and eax,0xff`); with pack[0]/[1] of [2] it forwards one
  from a register. Original byte slots at +1/+2 of a dword are the tell.
- **Per-arm statements**: each arm writes its own colourB; VC5 cross-jumps the identical
  suffix. A shared statement after the join sinks `top<<8` into the tail instead.
  Within an arm, assignment order steers the scheduler (top byte first).
- **Base/index order** `[model + idx]`: typed global (`BrCarModel *DAT_106ea398`, record
  array field) instead of a cast `void *` global. Pads/TU state were inert.
- **fld/fadd order** for `car[12] + eyeScale`: `volatile float eyeScale` (summand order,
  decl order, TU pads all inert).
- **Copy order** x,y,z source gave the original's x,z,y loads; the old x,z,y source was a
  compensation. Image gate "one symbol reaches N original addresses" findings catch
  field mixups that are byte-identical (bytes come from reference fill).
- Stale reloc_overrides rows are keyed to the OLD recomp offsets: delete all rows for a
  function when it goes byte-exact (513 here), then rerun the gate.

scratch tools (session): cc.sh (compile C variant from build/brally/win32/match/probe_bd4988/dc),
al.py --noaddr/--pairs, fr.sh (/FAcs frame), frameinv.py, namemap.py, slotmap.py.
Related: [bracestep-hand-transcription-2026-10-05](bracestep-hand-transcription-2026-10-05.md), [hand-transcription-only](../rules/hand-transcription-only.md).
