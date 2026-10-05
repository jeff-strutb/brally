# Guard shape decides prologue

> A multi-condition entry guard has TWO correct spellings and the return values pick which - && chain vs sequential early returns; got 3 byte-exact in one pass.

Proven 2026-09-03 on the sound-gate family (`BrSndG0B5DE8 && BrSndPDS &&
BrSndG18290FC`), full write-up in `docs/VC5-IDIOMS.md`.

- **Guard and body return the SAME constant** → three sequential
  `if (x == 0) { return 1; }` early returns. An `&&` chain instead nests the
  body, and VC5 **shrink-wraps** the callee-saved `push`/`pop` into that
  nested block; the original saves them in the PROLOGUE, interleaved with the
  first test. Multiset-identical, so triage scores it 0 reggap and files it
  as a colouring wall - it is not one. (0x1006BD70 `BrSndBankMute`.)
- **Guard and body return DIFFERENT values** → one `&&` chain wrapping the
  body. Early returns make VC5 tail-duplicate the guard exit - three
  `mov eax,1 / pop / pop / ret` copies instead of the original's single `je`
  target, +32 bytes. (0x1006B530 `BrSndChanBind`.)

Screen for the first case with `tools/fnmatch/screen_shrinkwrap.py` (same
push/pop bag, different first-save index). **Zero yield on the tagged pool**  - 
it is a T1-intake lever, not a family.

Second idiom from the same function: an explicit `shl R,3` (or `shl R,2`)
sitting next to a `lea` that already built a row index means **the element
type in our C is narrower than the original's**. `[R*8 + K]` with no shift =
the source indexes an 8-byte-element array. A `double` copied between memory
locations still emits two dword `mov`s, so the mov pair is NOT evidence of an
`int[]`. See also [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).
