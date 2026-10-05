# N64 introscreen frame layout

*Recorded 2026-10-05.*

> BrIntroScreen 0x8020686C T4 2026-10-05 (d03d3ece, 1459 -> 0 by hand): solve the IDO frame from ROM sp offsets (decl order incl. gbi _g temps), conditional-expression args, pointer-typed ROM offsets, unrolled chained clears

BrIntroScreen 0x8020686C (n64/src/menus/credits.c) T3 -> T4 on 2026-10-05, commit d03d3ece, image gate 665/0. Hand transcription; the 1459-word permuter residue fell in one session.

**Levers, in the order they closed it:**
- `(int)D_001DCD70` casts on link-time ROM offsets made uopt CSE the address across calls (sp spill). Declaring the callees with `char *rom` and passing the bare symbol makes the lui/addiu rematerialise after each call, as in the ROM. BrRomUnpack returns int.
- **Frame = declaration order, solved from ROM sp offsets.** IDO places locals top-down in source order: function-scope locals, then every block local AND every gbi macro's `Gfx *_g` block temp as encountered. Use ROM spill/home offsets (sh/lh of short voices, sw of mtx) to count slots between landmarks. Count the gfx commands in a stretch to get the _g temp count. uopt spill temps sit BELOW all cfe locals, so a ROM spill below the temps (pad at 0x24) means that value is a uopt temp (strength-reduced pointer from an indexed loop), not a declared local.
- A float that has no home in the ROM frame was a conditional expression inline in the call argument, not a local `z`. The same goes for `a = c ? x : 200` (the ROM's `b join` at the end of an arm with nothing after = ternary).
- Fixing the frame also fixed the FP colouring: the clock static's f16 vs f14 came from interference with the spilled-z web (diagnosed via `n64alloc force p1:w70=c28`, 756 -> 198).
- A fully unrolled clear with a base register and `level` storing the pitch's low-word register = explicit per-element `D[k].level = D[k].pitch = 0;` (a loop, chained or not, does not fully unroll).

Related: [n64-loadsave-uopt-2026-10-05](n64-loadsave-uopt-2026-10-05.md), [n64-carselect-handtranscription-2026-10-05](../functions/n64-carselect-handtranscription-2026-10-05.md), [n64-m2-session-2026-10-04](../log/n64-m2-session-2026-10-04.md) (homes).
