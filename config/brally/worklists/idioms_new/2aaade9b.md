# TOKEN 2aaade9b - new construct→bytes mappings

## Named width pins `imul` operand order (0x10059F10 BrBmpToRgba32)

- Construct: `int w = *(int *)(bm + 4); malloc(w * *(int *)(bm + 8) * 4);`
- Bytes: `mov eax,[esi+4]; push edi; imul eax,[esi+8]; shl eax,2`
- Distinguisher: bare `w * h * 4` commutes to `mov eax,[esi+8]; imul eax,[esi+4]` (2 displacement diffs, REGNORM 0+0). Named first operand forces orig load order.

## DL opcode, 1-arg, always-call (0x1001E770 BrDlCmdSetCombine)

- Construct: matching build takes the command pointer as arg0 (FogColour swap vs the port's `(pS, p)`). `w0 = *(int *)p; DAT_105d17ac = w0; w1 = *(int *)(p+4); DAT_105d17b0 = w1; FUN_1001e7a0(w0, w1); return p+8;`
- Bytes: `mov eax,[esi]; mov [DAT],eax; mov ecx,[esi+4]; push ecx; push eax; mov [DAT],ecx; call; add esp,8; lea eax,[esi+8]`
- Distinguisher: orig stores to globals not `pS->field`; always calls (no `if (pfn)`); cdecl `add esp,8`. Port NULL-guard + struct stores is `mov [R+I]` vs orig `mov [I]`.

## Scissor twins, 1-arg, stdcall grClipWindow (0x1001EB50 / 0x1001EBC0)

- Construct: two functions, not one helper. Re-deref each command dword (don't `w0 = *p`). Unsigned `shr` (not `sar`). `H = DAT_100a7518`. Stores then `grClipWindow(ulx, H-lry, lrx, H-uly)` stdcall (no `add esp`).
  - 0xED: `(w>>14)&0x3FF` / `(w>>2)&0x3FF`
  - 0xE2: `(w>>12)&0xFFF` / `w&0xFFF`
  - DAT_105d17bc=ulx, DAT_105ccfe0=H-uly, DAT_105d17b8=lrx, DAT_105d17c0=H-lry
- Distinguisher: wrapper-through-shared-decode is 21 B vs 103/97 B. cdecl would `add esp,10`; orig does not.

## `c = 0` before pool address (0x10023B10 br_dl_clip_reset)

- Construct: `c = 0; a = 0x105cd9c8; do { *(int *)a = c; c = a; a -= 0x28; } while (a >= 0x105ccff0);`
- Bytes: `xor ecx,ecx; mov eax,0x105cd9c8; mov [eax],ecx; mov ecx,eax; sub eax,0x28; cmp eax,0x105ccff0; jge`
- Distinguisher: `a = pool; c = 0;` emits `mov eax,imm; xor ecx,ecx` (7 diffs, REGNORM 0+0). Compare is signed `jge`.

## Guarded normalise: BrSqrtF + named x,y,z; scale-out is x87-schedule (0x100344D0)

- Construct: `float x=pV->x, y=pV->y, z=pV->z; len=BrSqrtF(y*y+z*z+x*x); if (len != 0.0f) { len=1.0f/len; pV->x=len*pV->x; ... } else { 0,0,1 }`
- Bytes through `test ah,0x40; jne zeros` match (fcom not fcomp; C3). Scale-out orig is `fld st0; fld st1; fxch; fmul [esi]`; recomp `fld [esi]; fld st1; fld st2; fmul st3` + leftover `fstp st0` (+4 B, REGNORM 3+1). `*=` / copy-assign / operand swap all compile identical - x87 operand selection, not source-reachable.
- Distinguisher: CRT `sqrt((double)...)` is FF 15 / extra fld; orig is E8 to 0x10002570 (`fld; fsqrt; ret`).
