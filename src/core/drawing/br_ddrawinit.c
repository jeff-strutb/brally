/* br_ddrawinit.c -- drawing: the loading-screen / bitmap-table setup.
 *
 * 0x100583C0 sits between BrFontFreeAndExit (0x10058360) and the sprite-font
 * draw (0x10058380). It allocates the font destination surface if needed,
 * optionally blits images\loading.bmp, then walks the {surface, path} table
 * at 0x10AC53E8 loading every named bitmap.
 *
 * RESIDUE 54B /O2, FIRSTDIV +0x110, REGNORM 2+1. First 272 B match. Two
 * leftovers: (1) after BrBmpLoadSurface orig stores h then reloads *p then
 * addesp; recomp reloads *p, addesp, then stores h. volatile store did not
 * reorder it. (2) loop back orig `jl body` falling into `return 1`; recomp
 * `jge success; jmp body` because the fail-sprintf block sits between the
 * loop and the success epilogue. goto-fail after return 1 still laid fail
 * in that hole. Same inversion class as 0x10027A70. */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

int BrSurfNew(int, int);
int BrBmpLoadSurface(char *, int, int);
int BrFontFreeAndExit(int);
int BrSprFontDraw(int, int, int, int, int);
int BrSurfFree(int);
int BrSurfSetColourKey(int, int);

extern int DAT_10ac5d84;
extern int DAT_10ac5dc4;
extern int DAT_10ac53e8;
extern int DAT_10ac53ec;
extern int DAT_10ac5c5c;
extern int DAT_10ac5c2c;
extern int DAT_100a7514;
extern int DAT_100a7518;
extern int DAT_105bc72c;
extern int DAT_100aad0c;
extern int DAT_100aad1c;
extern char s_images_loading_bmp_100ad71c[];
extern char s_DDraw_DoInit__loading_bmp_failed_100ad6f0[];
extern char s_DDraw_DoInit__Bitmap__d_failed_t_100ad6c8[];

typedef void (__fastcall *BrVt0)(int this);

/* WHAT IT DOES: set up the bitmap table the menus draw from. Allocates the
 * off-screen font surface on first call; when the "already inited" flag is
 * set, also puts up images\loading.bmp, draws the loading sprite and tears
 * that surface down. Then walks every {surface, path} slot, loading each
 * named BMP, bumping the live-surface count and giving it a colour key of
 * 0xFF00. A failed load of a still-named slot is fatal. Returns 1. */
/* @implements 0x100583C0 glide FUN_100583c0 */
int FUN_100583c0(void)
{
  char buf[0x100];
  int *p;
  int i;
  int h;
  BrVt0 pfn;

  if (DAT_10ac5d84 == 0) {
    DAT_10ac5d84 = BrSurfNew(DAT_100a7514, DAT_100a7518);
    if (DAT_10ac5d84 == 0) {
      BrFontFreeAndExit(DAT_105bc72c);
      return;
    }
  }
  if (DAT_10ac5dc4 != 0) {
    DAT_10ac53e8 = BrBmpLoadSurface(s_images_loading_bmp_100ad71c, 0, 0);
    if (DAT_10ac53ec != 0 && DAT_10ac53e8 == 0) {
      sprintf(buf, s_DDraw_DoInit__loading_bmp_failed_100ad6f0);
      BrFontFreeAndExit(DAT_105bc72c);
      return;
    }
    pfn = *(BrVt0 *)(*(int *)DAT_10ac5c5c + 0x20);
    pfn(DAT_10ac5c5c);
    BrSprFontDraw(0, 0, 0, (int)&DAT_100aad0c, DAT_100aad1c);
    pfn = *(BrVt0 *)(*(int *)DAT_10ac5c5c + 0x14);
    pfn(DAT_10ac5c5c);
    if (DAT_10ac53e8 != 0) {
      BrSurfFree(DAT_10ac53e8);
      DAT_10ac53e8 = 0;
    }
  }
  p = &DAT_10ac53ec;
  DAT_10ac5dc4 = DAT_10ac5dc4 + 1;
  i = 0;
  for (;;) {
    if (*p != 0) {
      h = (p[-1] = BrBmpLoadSurface((char *)*p, 0, 0));
      if (*p != 0 && h == 0) {
        sprintf(buf, s_DDraw_DoInit__Bitmap__d_failed_t_100ad6c8, i);
        BrFontFreeAndExit(DAT_105bc72c);
        return;
      }
      *(short *)&DAT_10ac5c2c = (short)(*(short *)&DAT_10ac5c2c + 1);
      BrSurfSetColourKey(h, 0xff00);
    }
    p = p + 2;
    i = i + 1;
    if ((int)p >= 0x10ac5874) {
      break;
    }
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
