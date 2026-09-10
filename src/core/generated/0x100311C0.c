/* Auto-generated from Ghidra decompilation — 0x100311C0 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Forward declarations for unknown functions/globals */
void FUN_10031140(int);
void FUN_10018a10(unsigned int, unsigned int);
void FUN_10018a40(int);
int FUN_10003320(char *);
int FUN_100032d0(int);
void FUN_10031b80(int *, int);
void FUN_100034c0(int *, int, int, int);
void FUN_100035e0(int);
void FUN_10032190(int *);
void FUN_10030f50(int *, char *, int);
void FUN_1006c910(void);
void FUN_1006c950(void);
void FUN_100314d0(int *);
void FUN_10034a70(int *, int *, int);
float FUN_100347f0(int *);
void FUN_1005a780(void);
void FUN_10069530(int);

extern char s_tracks__100b74c0[];
extern char *PTR_s_desert_trk_100b78c0;
extern char *PTR_s_cargfx_skytexdesert_lut4_100bb30c;
extern char *PTR_s_cargfx_skytexdesertn_lut4_100bb314;
extern char DAT_100aa348[];
extern char DAT_100aa378[];
extern char DAT_100aa3a4[];
extern int DAT_10ac080c;
extern unsigned char DAT_106eff08[];
extern int DAT_106eecd8;
extern int DAT_106eecdc;
extern int _DAT_106ec77c;
extern unsigned char *DAT_106b7c7c;
extern int DAT_106f0138;
extern int _DAT_100aa02c;
extern int DAT_100aa030;
extern int DAT_106eed38;
extern int DAT_106eed3c;
extern int DAT_118ed1f0;
extern int DAT_118eda10;
extern int DAT_118ee210;
extern int DAT_118ed210;
extern float _DAT_10077524;
extern float _DAT_10077528;

/* WHAT IT DOES: load one track: reset the handling data, set the segment
 * bases for the track heap, build "tracks/<name>.trk", read the 0x230-byte
 * header and the rest of the blob (capped at 4,000,000 bytes), run the
 * command fixup, read the four sky-texture files for the track, compute the
 * heap window globals, run the full endian/pointer fixup, then for every
 * 0x54-byte instance record derive the inverse scale of its matrix (marking
 * pure uniform scales with flag 0x20), and finish with the node mark pass
 * and the per-track surface table. Aborts with printf+exit(1) on a too-big
 * file, too many instances, or a header-size mismatch. */
/* T2 RESIDUE (770 orig / 775 recomp B, raw 11+9, regnorm 6+4, FIRSTDIV +0x1da):
 * byte-exact through the whole preamble (0..0x1d9); every remaining row is
 * inside the per-instance loop and cascades from one fork at the loop head —
 * the original sinks all three vector-init immediate stores BELOW the arg
 * pushes and forms arg3 with `add edx,esi`; ours interleaves x/y above the
 * pushes and uses lea. Downstream symptoms of the same fork: fdivr scheduled
 * before the g_6EED38 reload (ours after), `fld st(0); fmul mem` on the first
 * scale test (ours `fld mem; fmul st(1)`), base/index roles swapped in the
 * SIB bytes, and the flag OR left as a 3-insn RMW (orig folds it and keeps a
 * dead lea of pb at +0x4c). Not allocation-only: two insn shapes differ, so
 * not a t3 candidate yet.
 * @t4-pass 0x100311C0 1 2026-09-09 probes 6 bytes 775 insns 221 regions 1 rows 20 census no  (fn.py variants: int[3]/struct-float vector, dword-OR widening, pb[1] spellings, arg3 operand swap, int-punned stores)
 * @t4-pass 0x100311C0 2 2026-09-09 probes 5 bytes 775 insns 221 regions 2 rows 12 census no  (thin, recorded for honesty: initializer-declaration vector (block-scoped, both wirings), z/y/x store order, iVar8-destructive arg3, record-pointer promotion of all field sites -- the last is -27 B/FIRSTDIV +0x7: orig has NO rec pointer, fields are base+index SIBs, only arg3 is the destructive add) */
/* @implements 0x100311C0 glide BrTrackLoad */
void BrTrackLoad(int param_1)

{
  unsigned char *pbVar1;
  int uVar3;
  int iVar6;
  int iVar8;
  float fVar11;
  struct { float x; float y; float z; } local_40c;
  char local_400 [1024];

  FUN_10031140(param_1);
  DAT_10ac080c = 0x80025c00 - (int)DAT_106eff08;
  FUN_10018a10(0x80025c00, (unsigned int)DAT_106eff08);
  FUN_10018a40(1);
  strcpy(local_400, s_tracks__100b74c0);
  strcat(local_400, (&PTR_s_desert_trk_100b78c0)[param_1]);
  uVar3 = FUN_10003320(local_400);
  iVar6 = FUN_100032d0(uVar3);
  FUN_10031b80(&DAT_106eecd8, uVar3);
  if (4000000 < iVar6) {
    printf(DAT_100aa3a4, param_1, iVar6, 4000000);
    exit(1);
  }
  FUN_100034c0(&DAT_106f0138, 1, iVar6 + -0x230, uVar3);
  FUN_100035e0(uVar3);
  FUN_10032190(&DAT_106eecd8);
  FUN_10030f50(&DAT_118ed1f0, (&PTR_s_cargfx_skytexdesert_lut4_100bb30c)[param_1 * 0x5f], 0x20);
  FUN_10030f50(&DAT_118eda10, (&PTR_s_cargfx_skytexdesert_lut4_100bb30c)[param_1 * 0x5f] + 0x20,
               -1);
  FUN_10030f50(&DAT_118ee210, (&PTR_s_cargfx_skytexdesertn_lut4_100bb314)[param_1 * 0x5f], 0x20);
  FUN_10030f50(&DAT_118ed210, (&PTR_s_cargfx_skytexdesertn_lut4_100bb314)[param_1 * 0x5f] + 0x20,
               -1);
  FUN_1006c910();
  FUN_1006c950();
  _DAT_106ec77c = (int)DAT_106eff08 - DAT_106eecdc;
  DAT_106b7c7c = DAT_106eff08 + DAT_106eecd8;
  FUN_100314d0(&DAT_106eecd8);
  iVar6 = 0;
  _DAT_100aa02c = -1;
  DAT_100aa030 = -1;
  if (0 < DAT_106eed3c) {
    iVar8 = 0;
    do {
      local_40c.x = 1.0f;
      local_40c.y = 0.0f;
      local_40c.z = 0.0f;
      FUN_10034a70((int *)&local_40c, (int *)&local_40c, DAT_106eed38 + iVar8);
      fVar11 = FUN_100347f0((int *)&local_40c);
      if (fVar11 != _DAT_10077528) {
        fVar11 = _DAT_10077524 / fVar11;
        if (((fVar11 * *(float *)(DAT_106eed38 + iVar8) == _DAT_10077524) &&
            (*(float *)(DAT_106eed38 + 0x14 + iVar8) * fVar11 == _DAT_10077524))
           && (*(float *)(DAT_106eed38 + 0x28 + iVar8) * fVar11 == _DAT_10077524))
        {
          pbVar1 = (unsigned char *)(DAT_106eed38 + 0x4c + iVar8);
          pbVar1[1] |= 0x20;
        }
        *(float *)(DAT_106eed38 + 0x40 + iVar8) = fVar11;
      }
      iVar6 = iVar6 + 1;
      iVar8 = iVar8 + 0x54;
    } while (iVar6 < DAT_106eed3c);
  }
  if (0x800 < DAT_106eed3c) {
    printf(DAT_100aa378, DAT_106eed3c, 0x800);
    exit(1);
  }
  if (DAT_106eecdc != 0x230) {
    printf(DAT_100aa348, DAT_106eecdc, 0x230);
    exit(1);
  }
  FUN_1005a780();
  FUN_10069530(param_1);
}


#endif /* BR_MATCHING_BUILD */
