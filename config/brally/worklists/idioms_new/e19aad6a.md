# Idioms from lane e19aad6a (2026-08-31)

## Glide BrGbiTexRec is not the packed header layout

Header `BrGbiTexRec` packs w/h/flags at +0x08/+0x0A/+0x0C (64-bit
port). Glide orig: pTex +0x00, f04 +0x04, uint16 w +0x0C, uint16 h
+0x0E, flags +0x20. Distinguisher: `mov eax,[esi+0x20]; test eax,0x100000`
and `xor eax,eax; mov ax,[esi+0xe]` then `[esi+0xc]` vs
`[esi+0xc]`/`[esi+0xa]`/`[esi+0x8]`. Matching-build reads via
`(uint8_t*)pRec + disp`. Proven 0x100297F0 BrGbiTexCreate (196 B, MATCH /O2).

## Re-read pCmd->w0/w1 each SETTILE field; index the global tile array

Caching `w0 = pCmd->w0; w1 = pCmd->w1; BrGbiTile *p = &tiles[tile]`
emits a pointer (push esi, extra movs). Orig keeps pCmd in ecx, reloads
`[ecx]`/`[ecx+4]` per field, `shl eax,6` then `[eax+tiles0.fmt]`.
Spell `g_brTexScanTiles[tile].fmt = (pCmd->w0 >> 21) & 7` (no locals).
Proven 0x100295B0 BrGbiTexScanSetTile (244 B, MATCH /O2).

## Solid 4x4 fill is a signed pointer walk, not `for i in 0..16`

Orig: `eax = &texels[1]; [eax-1]..[eax+2]=cl; add eax,4; cmp eax,&texels[17]; jl`.
`p < end` on a `uint8_t*` is unsigned `jb`. Distinguisher: `7c` (jl) vs
`72` (jb) at the loop back-edge. Cast both sides `(int)p < (int)end`.
No pfn/pSt args - mode, texels, pTex, constructor are DAT_/IAT globals.
Proven 0x10029C70 BrGbiSolidTexBuild (91 B, MATCH /O2).

## Loop index live before the `n <= 0` early-out pins ebx

`if (n <= 0) return; i = 0;` tests first (`test eax; jle`) with no
`push ebx; xor ebx,ebx`. Orig: `push ebx; xor ebx,ebx; test eax; jle`.
Declare `int i = 0` *before* the early-out so ebx is dedicated and
zeroed on both paths. Inner `arg = 0` after the check becomes edi.
DAT_ pointer walk (esi += 0x2B68, owner at esi-0xDC4, slots at
imm+0x80) not `s17_car(i)`. Proven 0x1001C7A0 BrCarTableRemove
(112 B, MATCH /O2).

## /O2 merges identical `g=0; return;` - orig may keep three copies

G_SETOTHERMODE_L orig is three separate `cmp; jne; mov [g],0; ret`
then `xor ecx,ecx; cmp eax,ecx` for the v==0 / `test ah,0x18` fail.
`if (v==A) { g=0; return; } if (v==B) ...` CSEs to `je common_store`.
Distinguisher: three `c705 [g],0; c3` vs one shared store. Unrolling
does not survive /O2. 0x10029710 still DIFF.

## SETTILESIZE state=6 sits between lrs and lrt; /O2 hoists it

Orig: store lrs, reload w1, and 0xFFF, `mov [state],6`, store lrt.
`tiles[tile].lrs=...; state=6; tiles[tile].lrt=...` hoists state above
lrs (different objects, no alias). Comma and volatile store did not
block the hoist. Distinguisher: `c705 [state],6` at +0x3b vs +0x4a.
91/91 REGNORM 0. 0x100296B0 still DIFF.
