# VC5 idioms — task 3 (0x10071710 / 0x10014960 / 0x10040EB0 / 0x1001CF90)

Proven against BRGlide.dll this session. Merge into `docs/VC5-IDIOMS.md`.
Infer source from the bytes; never permute spellings.

## Proven MATCH

- **Param-reassign then pack, not interleaved convert-and-shift.** A 16-arg
  packer that calls a 1-arg converter per field keeps the first four results
  in ebx/esi/edi/ebp across the remaining calls and overwrites later args'
  own stack slots, then packs. Interleaved `w = (w << n) | (f(x) & mask)` is
  ~40 bytes short and never pushes ebp. Proven 0x1001CF90 BrRdpSetCombineLERP
  (440 B, MATCH /O2).

- **Consecutive-case switch 0 / 1 / K is `sub eax,0; je; dec; je; sub
  eax,K-1; je; lea eax,[ecx-base]`.** Same shape as BrRdpCCMux (0/1) with one
  extra `sub`. Ghidra's if-chain is 5 bytes long and 40 diffs. `default:
  return token - 1000` uses the original ecx (`lea eax, [ecx-0x3e8]`).
  Proven 0x1001D150 BrRdpACMux (43 B, MATCH /O2).

## Structural, not yet MATCH

- **0x10014960 BrSub_100173F0 (664 B, 25 diffs /O2, 2 regions).** Orig has no
  BrScreenGet/BrHudGetEnv: cViews, iView, the suppress flag and the race
  object are standalone globals. Position is `[obj+0xFF8]` of the same
  object as cSplits at +0xFA8, not a NULL-checked pointer. sprintf is
  dllimport, IAT CSE'd into ebp. `nudge = 0; switch (pos - nudge)` is
  `xor ebx,ebx; sub eax,ebx; je; dec; je; dec; je` so 3rd-place leaves
  nudge at 0. String literals (`"L"`, `"%%y1%s%d/%d"`, `"%d"`) not
  `const char *` globals (push offset vs load pointer). First 0x21f bytes
  (543/664) match. Residue is `w+nudge` vs `nudge+x` association on the
  full-screen suffix (`add edx,ebx; lea [edx+esi+3]` vs `add ebx,esi; lea
  [ebx+edx+3]`) and /3 vs y-push interleave on the split path — allocator
  wall, size 672 vs 664.

- **0x10071710 BrInputIsDown (695 B, 495 diffs /O2).** Nested `if (w <= P)
  { if (w != P) { if (w == Q) } else { P } }` reproduces orig's inner
  compacted `cmp; jg; je` nodes and the last-three linear `==` chain. It
  does NOT uncompact the root: VC5 still emits `cmp; jg; je` (flags
  reused) where orig re-compares (`cmp; jg; cmp; je`). A `w2 = w` copy
  does not force the second cmp onto a different register. Remaining is
  ebx vs edi for the axis scale (orig push ebx first; ours push esi/edi)
  — same coloring wall as BrTex3dRegister. `uint8_t r = 0` before the
  table lea matches orig's `xor al,al` between the two pushes.

- **0x10040EB0 BrUiNavCtlHit (587 B, 398 diffs /O2y, 6 regions /O2).**
  Function is thiscall (`mov esi, ecx`, this = pCtl); header cdecl
  `(nav, ctl)` must be `#define`'d aside for the matching TU. Cursor,
  three hot rects, ordinal words and the 9 activity flags are standalone
  globals, not `pNav->`. BrIsAnyActive is 0-arg cdecl; page-select is
  thiscall on `pCtl->pOwner->pCur` (`mov ecx, [eax+0x64]; call`). First
  two instructions match (`push ecx; push ebx`). Missing `push ebp` and
  the early `mov [esp+0x10],0` (fZero slot + ebp as a general register
  for the 19* lea pair on the miss path). Hot-rect operand order is
  `cmp left, x; jg` — preload left and top before the cursor load to
  match orig's ecx/edx pair. `*(unsigned char *)&flags |= 0x22` spills;
  orig is `or al, 0x22` on the dword in eax.

## Generator candidates

1. **Ghidra interleaved pack `w = (w << n) | (f(arg) & mask)` whose orig
   calls every converter first.** Rewrite as `arg = f(arg);` for each
   argument in order, then the pack. Distinguisher: orig `push ebx; push
   ebp; push esi; push edi` then 16 calls with the first four results in
   those four registers; interleaved form is short and 3-register.

2. **Ghidra if-chain on 0 / 1 / K → consecutive-case switch.** Bytes
   `83 e8 00 74; 48 74; 2d xx 00 00 00 74` vs `83 f8 00 74; 83 f8 01`.
   Already the CCMux shape; ACMux adds the third case.

3. **Getter call vs standalone global.** Orig `mov eax, [g_cViews]` not
   `call BrScreenGet; mov eax, [eax+8]`. When the first divergence is a
   call the port documents as a getter, drop it.

## N64 twins used

None. CombineLERP is libultra `gDPSetCombineLERP` but the x86 call-then-pack
shape named the source; the N64 twin is a macro and would not have helped
the packer. Input / HUD / UI nav are PC (DirectInput / sprintf / thiscall).
