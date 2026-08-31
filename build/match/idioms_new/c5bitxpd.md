# 0x1001E9F0 br_dl_fillcolour — 5-bit RGB and/or expand

## Landed: 110 B, MATCH /O2

Source is the 5→8 widen, not the xor-blend the bytes show:

```
DAT = (unsigned char)(((w >> s) & 0xF8) | ((w >> t) & 7));
```

VC5 lowers that to `mov ecx,w; mov edx,w; shr; shr; xor dl,cl; and dl,7; xor dl,cl`. Spelling the xor-blend (`a ^ ((a ^ b) & 7)` with `unsigned char` temps) compiles to a 2-register in-place shr: no `push esi`, one `mov edx,ecx`, REGNORM 0+4 / -6 B.

Per-channel `w = *(unsigned *)(p+4)` reload keeps p in eax (`add eax,8`). Blue: byte `& 0xFE`, `<< 2`, dword `>> 3` `& 7`. Alpha: `(w & 1) ? 0xFF : 0` (`neg; sbb; and 0xff`), not `0 - (w & 1)`.
