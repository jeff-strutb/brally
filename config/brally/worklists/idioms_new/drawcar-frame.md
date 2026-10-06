# BrCarDrawVehicle (0x1000A110) - frame + missing-logic idioms

Proven 2026-08-31 against BRGlide.dll. Not yet byte-exact; these are
source-phrasing → bytes observations from a 52→49 region drop.

## Frame is `sub esp, 0x4c` with ebp as zero-reg

Original prologue:

    sub esp, 0x4c
    push ebx
    mov ebx, [esp+0x54]    ; pCar
    push ebp
    xor ebp, ebp           ; zero-reg, 154 uses (push ebp / put w1=0)
    mov eax, [ebx+0xf08]
    push esi
    cmp eax, ebp
    push edi
    je ret

A 0x44 frame meant missing locals (pSkyAng, reflection tile words,
lodBias-arg reuse). Growing the transcribed body to include the
reflection pass and the 4-pool specular setup produced `sub esp, 0x4c`
and `xor ebp,ebp` without dummy locals.

pCar-in-ebx vs pCar-in-edi is still open: recomp saves all four
callee-saved first, then `mov edi, [esp+0x60]`. Orig interleaves the
arg load after `push ebx`. ebx is claimed in the recomp by the hoisted
`mov ebx, 1` of the +0x290C flag; orig uses edx for that 1.

## Model pointer is the global, lod*40 is lea/shl

`model = BrG_6C3308` then `model[off]` is a cached pointer. Orig reloads
`[0x106ea398]` at every DL/tex site and compares `[edx+eax+0x8024]`
against ebp. Spell `(unsigned char *)BrG_6C3308 + off`.

`lod * 40` is `lea eax,[eax+eax*4]; shl eax,3` i.e.
`(lodBias + lodBias*4) << 3`. An `imul 40` or a once-hoisted `* 40`
local is extra.

## Tried and rejected this session (region count went up)

- Colour pack in all three arms + `int two = 2` + pointer-sub: 41→52, RAW exploded.
- Full lights rewrite (re-read iCar per store, delay lodOff until detail): 41→58, frame 0x4c→0x44.
- Dir0 via float temps + integer x: still 41; got `fld y` but not `fld z; fld y`.
- Pointer-sub ternary for distNear: already what if/else emits (sbb/neg 4).

Lights byte stores orig are `mov byte [ecx*8 + g_BrDrawLights+0x10], al` after re-read iCar. A dest pointer is `mov byte [edi+0x10]`. Landing the scaled-index form without shrinking the frame is still open.

## Two-constant ternaries (next, don't shrink the frame)

Orig has 5 neg/sbb pairs (10 insns). Spelling these as `c ? K1 : K2`
matches all 10, but dropping the if/else cull locals shrinks the frame
to 0x48 (FIRSTDIV back to +0x2, 49→50/51 regions). Restore 0x4c first
via the overlapping colour-pack slots, then land the ternaries:

- 0xAA47: `(kind != 2) ? 0xC8000000 : 0x0C080000` (sub dl,2 / neg / sbb)
- 0xAA89: `(diff ^ BrG_6C1174) ? 0x1000 : 0x2000` and the inverse for B6
- 0xAB55: distNear ? `(car - player) ? 0x112038 : 0x112078` : 0x112230
- 0xB986: `g_BrDrawReflectFlag ? 0xC0000 : 0x40000`

## N64

This PC function has no debug strings. TGR `sizeof(UltraCarHeader)=%d`
xrefs a loader at ~0x8024F1B4, not the draw twin. Missing chunks were
recovered from the x86 (G_DL 0x100A9FC8/0x100A9F00, four pool allocs,
reflection 0xB685-0xB925).

## Arg slot reuse

`lodBias` at [esp+0x64] is overwritten with the computed lod
(`add esi, [esp+0x64]; mov [esp+0x64], esi`). Later the same slot is
a saved command pointer in the reflection pass. Spell `lodBias += lod`
and use lodBias thereafter; a separate `lod` live across the function
adds a slot.

Re-read `*(int32_t *)(car+0x140)` and `BrG_6C3308` at every use. A
cached iCar/model local is extra slots and extra `mov [esp], r`.

## Four pool allocs, not three

Orig at 0xA6F6:

    call 0x10062500   ; BrSub_10069490, result discarded
    call 0x10062550   ; BrPool16Alloc → BrSkyAngles (s0/t0 used in reflection)
    call 0x100625A0   ; BrPool32Alloc → BrLightPair
    call 0x100625A0   ; BrPool32Alloc → specMem (G_MOVEMEM 0x0384/0x0382)

The matching mtx path is `p = BrSub_10069490(); slot[iCar] = p;
BrGuMtxStore(src, slot[iCar])` with iCar re-read, no NULL test.

## Missing G_DL after AndAngles

`if (dist > 10.0f) put(G_DL, BrG_0AA838); else put(G_DL, BrG_0AA770);`
(0x100A9FC8 vs 0x100A9F00). 10.0f is rdata 0x100771B4.

## F2 settile is `-33 - ftol(...)`

Not `tile+2` / `tile+0x7E`. Orig:

    ftol(player+0x2718 * -20.3718318939209f)
    ecx = 0xFFFFFFDF - eax    ; -33 - tile
    lo = ecx+2; hi = ecx+0x7E

## Reflection pass 0xB685-0xB925 is real code

Gated on reflect-enable, wheel-alt==0, 6C6624==0, not
(flag290C && !6C661C), not (6C661C && car==player), suppress==0,
i29B4==0. Body is combiner + DC tex (6C661C ? 0x1184C474 : 0x1184C480)
+ a G_SETTILE packed from pSkyAng->s0/t0 + G_DL of model+lodOff+0x803C.

## Wheels call is 1-arg cdecl

`push ebx; call 0x10009C10; add esp, 4`. A 2-arg prototype pushes a dummy.
