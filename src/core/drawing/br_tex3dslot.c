/* br_tex3dslot.c -- drawing: allocate one Glide TMEM slot.
 *
 * 0x10028200 is the allocator 0x10027710 calls. The 0xD8-stride table at
 * 0x10661840 is the same one BrTex3dDownloadAt / FUN_10028420 / FUN_100281c0
 * already walk.
 *
 * RESIDUE, FIRSTDIV +0x0. Orig prologue is `mov eax,[idx]; sub esp,8;
 * cmp eax,0x400; push ebx,ebp,esi,edi; jae fail-at-end`. Recomp emits
 * `sub esp,8` first, inverts the 0x400 test to `jb ok` with an inline
 * fail, and shuffles the GrTexInfo store order. goto-fail helped the
 * TMEM-full path only. Not a first-compile leaf -- needs a store-order
 * pass against the orig fill block at 0x100282D0. */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)

int __stdcall grTexTextureMemRequired(int, void *);
int __stdcall grTexMaxAddress(int);

extern unsigned int DAT_105d17ec;
extern int DAT_10661830;
extern int DAT_10661834;
extern char DAT_10661840;
extern char DAT_10661844;
extern char DAT_10661848;
extern char DAT_1066184c;
extern char DAT_10661850;
extern char DAT_10661854;
extern char DAT_10661858;
extern char DAT_1066185c;
extern char DAT_10661860;
extern char DAT_10661864;
extern char DAT_10661868;
extern char DAT_1066186c;
extern char DAT_10661870;
extern char DAT_10661874;
extern char DAT_10661878;
extern char DAT_1066187c;
extern char DAT_10661880;
extern char DAT_10661884;
extern char DAT_10661888;
extern char DAT_1066188c;
extern char DAT_10661890;
extern char DAT_10661904;
extern char DAT_10661908;
extern char DAT_1066190c;
extern char DAT_10661910;
extern char DAT_10661914;
extern float DAT_1007745c;

/* WHAT IT DOES: claim one 0xD8-stride Glide texture slot. Fills the
 * GrTexInfo, asks the card how much TMEM the mip chain needs, then tries
 * the TMU's low water-mark and if that would pass 2MB or the TMU's max,
 * the high water-mark. Returns the new slot index, or -1 if the table is
 * full (1024) or the TMU is out of memory. */
/* @implements 0x10028200 glide FUN_10028200 */
int FUN_10028200(int tmu, unsigned int lod, int a2, int a3, int a4, int a5,
                 int a6, int a7, int a8, int a9, int a10, int a11,
                 int a12, float bias, int a14)
{
  unsigned int idx;
  int req;
  int start;
  int next;
  int off;
  char *info;

  idx = DAT_105d17ec;
  if (idx >= 0x400) {
    goto fail;
  }
  off = (int)(idx * 0xd8);
  info = (char *)&DAT_10661904 + off;
  *(int *)info = a6;
  *(int *)((char *)&DAT_10661908 + off) = a7;
  *(int *)((char *)&DAT_1066190c + off) = a8;
  *(int *)((char *)&DAT_10661910 + off) = a4;
  *(int *)((char *)&DAT_10661914 + off) = 0;
  req = grTexTextureMemRequired((int)(lod & 0xff), info);
  start = *(int *)((char *)&DAT_10661830 + tmu * 8);
  next = start + req;
  if ((unsigned int)next < (unsigned int)grTexMaxAddress(tmu)
      && (unsigned int)next < 0x200000u) {
    *(int *)((char *)&DAT_10661830 + tmu * 8) = next;
  } else {
    start = *(int *)((char *)&DAT_10661834 + tmu * 8);
    next = start + req;
    if ((unsigned int)next >= (unsigned int)grTexMaxAddress(tmu)) {
      goto fail;
    }
    *(int *)((char *)&DAT_10661834 + tmu * 8) = next;
  }
  *(int *)((char *)&DAT_10661840 + off) = 0;
  *(int *)((char *)&DAT_10661844 + off) = 1;
  *(int *)((char *)&DAT_10661848 + off) = a2;
  *(int *)((char *)&DAT_1066184c + off) = a3;
  *(int *)((char *)&DAT_10661850 + off) = a8;
  *(int *)((char *)&DAT_10661854 + off) = 0;
  *(int *)((char *)&DAT_10661858 + off) = a4;
  *(int *)((char *)&DAT_1066185c + off) = a5;
  *(int *)((char *)&DAT_10661860 + off) = a12;
  *(int *)((char *)&DAT_10661864 + off) = a11;
  *(int *)((char *)&DAT_10661868 + off) = a9;
  *(int *)((char *)&DAT_1066186c + off) = a10;
  *(int *)((char *)&DAT_10661870 + off) = 0;
  *(int *)((char *)&DAT_10661874 + off) = 0;
  *(int *)((char *)&DAT_10661878 + off) = (int)(bias * DAT_1007745c);
  *(int *)((char *)&DAT_1066187c + off) = a6;
  *(int *)((char *)&DAT_10661880 + off) = a7;
  *(int *)((char *)&DAT_10661884 + off) = tmu;
  *(int *)((char *)&DAT_10661888 + off) = (int)(lod & 0xff);
  *(int *)((char *)&DAT_1066188c + off) = start;
  *(int *)((char *)&DAT_10661890 + off) = a14;
  DAT_105d17ec = DAT_105d17ec + 1;
  return (int)idx;
fail:
  return -1;
}

#endif /* BR_MATCHING_BUILD */
