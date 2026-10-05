# Oracle resolution advances

*Recorded 2026-09-16.*

> Four A5-oracle resolution advances that let it RUN many more functions - VA-ownership obj selection, demangled Class::Method keying, __ftol/static-CRT-helper VAs, and $T .rdata-constant soundness.

**2026-09-16: four resolution fixes to the A5 oracle (tools/t3b_verify.py,
t3b_env.py), each unblocking a whole class of functions the oracle previously
refused. Extends [upgrade-byteshape-t3-to-a5-proven](upgrade-byteshape-t3-to-a5-proven.md) and
[t3-is-not-a-shortcut-for-fresh-transcriptions](t3-is-not-a-shortcut-for-fresh-transcriptions.md) - the work is making the
oracle RUN, not the byte grind.**

1. **Select the obj by VA OWNERSHIP, not size (3e1b932b).** `_obj_index` keys by
   symbol NAME, and a name is shared across TUs (a Glide fn + its D3D twin; a
   superseded C-lane slice + the cpp-lane file that took the VA over). Picking
   the size-closest candidate tested the WRONG transcription - BrTimeUpdate
   0x1006E360 (68B orig) matched a 64B same-named symbol in slice1_09 over its
   own 80B obj and FALSE-DIFFed. Fix: `_va_owner(va)` reads report.csv `file` →
   owning obj basename; report_cpp.csv OVERRIDES it for cpp-lane VAs (their objs
   carry sweep/probe suffixes like `0x..._sweep_*.obj`, so match by the
   `obj_cpp` DIRECTORY, not basename). Size-closest is fallback only.

2. **Parse + key C++ methods by Class::Method (5099b80b, dbddd930).**
   parse_signature now matches `rettype Class::Method(params)` (the `::` can't
   sit in the return-type run) → __fastcall with an implicit leading `this` ptr
   (distinct from BR_THISCALL1, which spells `this` as an explicit param). AND
   `_obj_index` indexes each method obj under its DEMANGLED `Class::Method` name
   (a minimal MSVC demangler for the plain `?method@scope@@` form) as well as
   the mangled name - because `one` uses ONE `--name` for both the obj lookup
   (needs mangled) and parse_signature (greps source, needs `Class::Method`).
   So invoke a method with its SOURCE spelling `Class::Method`, never the
   synthetic report name (`BrRippleApply_1000C4E0`), which matches neither.
    Keying the OBJ by demangled name is only HALF of it (51bb1b21): the
   function-SYMBOL match in reloc_fill.fill_function AND t3b_env's
   unresolved_symbols/resolve_bytes also matched via `_undecorate` (which does
   NOT demangle C++), so called with the demangled name neither LOCATED the
   function -- fill_function returned None, unresolved_symbols returned []. Now
   a shared `reloc_fill.func_symbol_matches` (raw / _undecorate / demangled) is
   used at all three sites.

   **Misattributed-None lesson (51bb1b21):** fill_function returns None for
   FOUR different failures -- function symbol not found, size overrun,
   unresolved reloc, unhandled reloc type -- but resolve_bytes reported them ALL
   as "a relocation names a symbol with no known address", and unresolved_symbols
   only detects the third. That combination hid the real bug (symbol-not-found)
   as a phantom unresolved reloc. fill_function now takes an optional `reason`
   list so the true cause is reported. When resolve_bytes and unresolved_symbols
   DISAGREE, suspect the shared error message is lying about which failure it is.

3. **Map static CRT helpers to their image VA (1a87198f).** `__ftol` (every
   float→int conversion) is linked INTO the image, not a DLL import, so it has
   no IAT slot and resolve_bytes refused every float fn. `_CRT_HELPER_VA =
   {'ftol': 0x10074560}` in t3b_env augment_maps (same VA x87emu models). Add
   siblings there as they appear.

4. **$T .rdata-constant soundness (brally-ee, 50a62fb5).** A `$T<n>` float const
   is a VALUE read: resolve to ANY byte-identical image copy (value is value  - 
   sound). A ZERO const needs a mapped zeroed VA (find_bytes alone lands in the
   unmapped PE header → None). But a DATA GLOBAL needs TRUE address IDENTITY  - 
   resolving it to a value-equal-but-wrong object manufactures a false
   DIFF/EQUIVALENT; resolve those from the source `/* 0x<VA> */` annotation,
   GUARDED against a stale annotation the g_<hex> learned map contradicts.

**Diagnosis discipline that paid:** two "substituted bytes bury <neighbour>"
reports were NAME errors (a wrong twin name → wrong-size obj → bury), NOT
obj-selection bugs. Verify with the CORRECT name before blaming the oracle.

Net across the tree this session: certified EQUIVALENT 36 → 43. See also
[oracle-runs-orchestrators](oracle-runs-orchestrators.md), [equivalence-oracle-in-image-2026-09-10](equivalence-oracle-in-image-2026-09-10.md).
