# Refslot gate

*Recorded 2026-09-28.*

> 2026-09-28 the M2 image gate now checks the relocation slots it fills from the original (tools/refslot_check.py); DAT_ and @cpp_symbol names resolve by name; struct-of-scattered-globals and C++ stand-in callees fixed; how to fix without breaking bytes

What changed (2026-09-28, commits 9481a4c5..14411bea):
- `tools/refslot_check.py` runs inside `tools/image_build.py` for BRGlide: every slot filled from the
  original's dword is checked (own-.text labels by offset, literals/initialised data by content,
  every symbol -> ONE original base). Any finding FAILS the gate. audit.py C3 proves it can fail.
- `reloc_fill.resolve` reads `DAT_XXXXXXXX` addresses from the name; `relocmap.load_maps` adds every
  C++ body's `@cpp_symbol`; `config/globals_glide.csv` (surveyed names) now exists. audit C/C2.
- Fixed: 2 certified T3 (BrCarDrawVehicle rain byte swap = reloc_overrides role-swap rows;
  BrEnvEmit x87 rounding = volatile floats at the original's store points); 4 data-table bytes fenced
  (T4 1311->1307); string/float literal contents; struct gatherings (g_s17, g_br73, g_menu, g_hud,
  g_screen, g_weather, g_BrDPlay, g_text, g_brFfb, g_BrLoad, g_BrCharMap) split into DAT_ externs via
  per-field macros in the matching build; 48 C++ stand-in callees -> real symbols.

**How to apply (traps):**
- `verify.py` reports "CHANGED (code)" when only DIR32 addends move (struct offset -> 0): judge by
  the image gate, not verify.
- Adding file-scope externs flips later functions' x87 both-memory fmul roles: the FIRST declaration
  order decides (later-declared takes fld). Re-declare constants ahead of variables in the original
  order. A reloc-masked sweep cannot see this; name resolution + image gate can.
- If separating a function's fields moves its codegen, try parentheses first ((c*g)*dt fixed
  StepWind); else keep the struct read for that one function (BrDPlayThreadProc hQuit).
- Read the WHOLE gate output: a slot-finding line can hide a differing-bytes failure (ef3a77cf shipped
  a 36-byte image failure that way, fixed in 43b179fa).
- Consistent single-address C++ names (operator new, g_cur, ctors...) still resolve by reference-fill:
  correct behaviour, but not linkable until defined -- remaining work, not claimed done.

Related: [gap-sessions-audit-2026-09-28](gap-sessions-audit-2026-09-28.md), [lockstep-oracle-arbitration](lockstep-oracle-arbitration.md), [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md).
