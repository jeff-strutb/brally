# Brtex3dexpand doubling lever

*Recorded 2026-09-10.*

> SUPERSEDED 2026-09-16 - 0x100250D0 BrTex3dExpand is CERTIFIED T3 (br_tex3d_expand.c). The Gate-A-distance verdict here (short by 2/29 rows) is obsolete. Keep only for the width-doubling ternary lever as a T4 idiom.

The project lead named the VA on 2026-09-10, satisfying project rule 11.

**THE LANDED LEVER, and the method that found it.** Rank the residue by
FAMILY, then pick the family whose count is exact and even, and find its sites
by counting the same instruction form on both sides:

    lea R,[R+R]   original 19, ours 13   -> the 6 missing `add R, R` rows

Listing the offsets showed the original's were in NINE PAIRS and ours were
three pairs plus six singletons. The nine sites are the nine `param_8` mirror
blocks, which each compute the width doubling twice -- once to step the source
pointer back, once as the copy count. **The original RE-MATERIALISES `2*w` at
both uses.** Three of our blocks already spelled it as a repeated ternary and
matched; the other six spelled it as an if-assign
(`x = w * 2; if (param_7 == 0) x = w;`), which lets VC5 compute the doubling
once and COPY it (`mov eax,ecx`). Converting all twelve statements to
`(param_7 != 0) ? w * 2 : w` closed the family: rows 70 -> 62, key-6 masked
regions 47 -> 43. Inverting the ternary's polarity gives the gain straight
back (70, and 82 on one set of sites) -- that is the corroboration.

 **Bytes move the WRONG way on this lever** (8,443 -> 8,412 against 8,480).
Rank by the multiset and the masked map; this file's own 2026-09-05 note
already says so and it is right.

**Gate B now PASSES** (two counted passes, 24 compiles, corpus MISS as the
census). Gate A is the only thing left: A2 62 against a limit of 60.2, A3 29
unpaired, A4 263 B uncompared at key 6 (and key 6 is the key this function's
dossier proves mis-syncs it -- t3.py scales the key 3..6 by size and cannot be
told to use 10).

**Both remaining families are diagnosed and are ALLOCATION, not spelling:**

- (A) ~12 rows, the byte-vs-dword nibble merge in blend body 3. The byte's
  only use is `uVar19 = (unsigned int)bI4inten`, so VC5 folds the merge into
  dword form; the original merges in byte registers, homes the byte and
  reloads it as a dword + `and 0xff` (the byte-slot widening idiom). Giving
  the byte a SECOND use so it cannot fold is DEAD both ways tried (at alpha,
  where the original's intensity dies: 74 rows; at red: 86).
- (B) the rest, constants the original pins in registers. At 0x6f5 it does
  `xor eax,eax; mov [counter],eax; je; cmp ebx,eax` -- the guard's 0 lives in
  a register and serves both the counter store and the bound test; we emit
  `mov [counter],0` and `test ebx,ebx`.  The obvious fix is DEAD: the
  comma-guards already read `(counter = 0, ..., 0 < n)`, so spelling the bound
  as `counter < n` is semantically identical and names the register -- but VC5
  CONSTANT-PROPAGATES the just-assigned 0 and folds it straight back. Six
  guards converted, byte-for-byte inert. Same story as the pinned 1 at 0x15a4.

Family (B) is the same wall as [callee-saved-zero-web-class](callee-saved-zero-web-class.md) and
BrCtlAiBody's R1/R2. Family (A) is what the parallel session's new t3.py
zero-extend canon class is aimed at.

Related: [gate-a-distance-survey-2026-09-10](../log/gate-a-distance-survey-2026-09-10.md), [brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md),
[byte-slot-idiom-cracked](byte-slot-idiom-cracked.md), [t3-certified-standard](../rules/t3-certified-standard.md).
