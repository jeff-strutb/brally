# VC5 codegen idioms — read before matching any function

Every entry here was proven by a byte diff against BRGlide.dll, not inferred.
Add to this file whenever a new construct→codegen mapping is proven; this is
how per-function cost drops over time. Each idiom is solved ONCE.

## The method: infer the SOURCE from the bytes, never permute spellings

Trial-and-error spelling permutation is the slowest possible approach. When a
diff looks like register or width noise, ask what TYPE or STRUCTURE the
original source must have had to force those bytes.

Worked example (BrCarStatePack, 738 B, matched 2026-08-22): the original did
`mov bl,al` (byte) after a call where we emitted `mov ebx,eax` (dword). Not a
spelling problem — the original PROTOTYPE returned a char-width type.
Changing `int32_t BrFixPackS6Q7Neg/Level/U8Range(float)` to `int8_t` matched
the caller AND flipped a helper to match for free.

## Proven idioms

- **Flag-from-float:** `(f != 0.0f)` → `fld; fcomp [0.0 const]; fnstsw ax;
  test ah,0x40;` + jne/xor-vs-mov-1 branch pair. `(f != 0) ? 0x80 : 0` picks
  the constant into the mov.
- **Hoist order across calls is source left-to-right.** Subexpressions that
  must survive a call (flags into ebx/ebp, spills into stack slots) are
  hoisted in left-to-right source order. To hoist A before B, A's term must
  come first in the expression — or be a prior statement.
- **Shift distribution:** `(a*2 | b) << 1` gets the outer shift distributed
  into lea-scales. `((a << 1) | b) << 1` keeps the literal `shl`.
- **Accumulate stores:** `b[k] = x; b[k] |= y;` emits one store per statement
  with the value register-forwarded (no reloads). `t |= X; b[k] = t` leaves
  the or result in t's register; `b[k] = t | X` leaves it in al. The operand
  order of `|` itself is canonicalized — swapping does nothing.
- **Byte ops need byte types end to end:** `and bl,0x3f` / `shl al,6` come
  from uint8_t-typed locals and char-width return types, not from masks on
  ints.
- **Direct struct reads vs float locals:** both-memory products emit
  `fld [mem]; fmul [mem]`. A float local assigned from memory is copied via
  INTEGER mov to a stack slot and multiplied from the slot. A function mixing
  both (squares from locals, cross terms direct — BrRbBuildMatrix) shows the
  mix in the bytes; transcribe it, don't normalize it.
- **`(int)float` is always a `__ftol` CALL** (truncating). A bare `fistp`
  with no fldcw around it (round-to-nearest) CANNOT come from VC5 C —
  `/QIfist` does not exist in VC5 (warning D4002). Such code was inline
  `__asm` in the original and needs an asm-hybrid mechanism (none in the
  tree yet). First known case: BrDlCmdVtx 0x10021A20.
- **Statement reordering of a pure float DAG compiles to IDENTICAL bytes.**
  VC5 canonicalizes the DAG, so x87 scheduling residue (operand selection:
  `fld st5; fmul [mem]` vs `fld [mem]; fmul st5`; spill-slot count; fst vs
  fstp) is allocator-internal and NOT source-reachable. Documented wall
  class — see divergence-class triage. Do not burn time here.

## Cost model (measured, 2026-08-22 timed test)

Size is not the cost driver — code shape is. 738 B of int/call-heavy code
matched in ~10 min; a 165 B float function (BrVec3Project) is permanently
stuck at 30 diffs of scheduling residue. Int/call-heavy code sustains
5–10 min/function at any size once the file's context is loaded. The float/
x87 cluster carries the walls and is a separate workstream.
