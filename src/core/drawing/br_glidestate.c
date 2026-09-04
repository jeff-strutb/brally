/* br_glidestate.c -- drawing: putting the 3dfx card into a drawing state.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_17.c, an address batch and not a module.  Everything
 * here talks to Glide directly: the standard render state a frame opens
 * with, the fog table, binding one texture slot to the card, and emptying
 * the card's texture table again.
 *
 * slice2_17.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c), so nothing is trimmed here
 * on the grounds that it is unused.
 */
#ifdef BR_MATCHING_BUILD
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_17.h"
#ifdef BR_MATCHING_BUILD
#undef BrPtrListContains
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD

extern int DAT_105d1718;
extern int DAT_106ed6a8;
extern int DAT_106ed6b0;
extern int BrG_6C661C;
extern int BrG_6C6624;
void __stdcall guFogGenerateLinear(int *table, float nearZ, float farZ);
void __stdcall grFogTable(int *table);

/* WHAT IT DOES: choose the fog distances for the current situation and hand
 * them to the fog table generator. Three of the four cases share the same
 * wide range and only the ordinary in-race case pulls the fog in close,
 * which is why the branch chain looks redundant. */
/* @implements 0x10023AA0 glide FUN_10023aa0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_10023aa0(void)

{
  if (DAT_106ed6a8 != 0) {
    if (BrG_6C6624 != 0) {
      guFogGenerateLinear(&DAT_105d1718,-100.0f,300.0f);
    }
    else if (DAT_106ed6b0 != 0) {
      guFogGenerateLinear(&DAT_105d1718,-100.0f,300.0f);
    }
    else if (BrG_6C661C != 0) {
      guFogGenerateLinear(&DAT_105d1718,60.0f,200.0f);
    }
    else {
      guFogGenerateLinear(&DAT_105d1718,-40.0f,220.0f);
    }
  }
  grFogTable(&DAT_105d1718);
  return;
}

extern int DAT_105ccbd0;
extern int g_BrFpsScreenH;
extern int g_BrFpsScreenW;
void __stdcall grClipWindow(int, int, int, int);
void __stdcall grDepthBufferMode(int);
void __stdcall grDepthBufferFunction(int);
void __stdcall grDepthMask(int);
void __stdcall grBufferClear(int, int, int);
void __stdcall grCullMode(int);
void __stdcall grAlphaCombine(int, int, int, int, int);
void __stdcall grAlphaTestFunction(int);
void __stdcall grAlphaTestReferenceValue(int);
void __stdcall grTexFilterMode(int, int, int);
void __stdcall grConstantColorValue(int);
void __stdcall grColorCombine(int, int, int, int, int);
void __stdcall grTexCombine(int, int, int, int, int, int, int);

/* WHAT IT DOES: put the 3dfx card into the game's standard drawing state for
 * a frame -- scissor box to the full screen, depth buffering on and cleared,
 * culling off, and the default alpha and colour combiners. Called once at
 * the top of rendering so nothing inherits state from the previous frame. */
/* @implements 0x1001DFB0 glide FUN_1001dfb0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1001dfb0(void)

{
  int uVar1;
  int uVar2;
  int uVar3;
  int uVar4;
  
  grClipWindow(0,0,g_BrFpsScreenW,g_BrFpsScreenH);
  grDepthBufferMode(2);
  grDepthBufferFunction(7);
  grDepthMask(1);
  grBufferClear(0,0,0xffff);
  grCullMode(0);
  grAlphaCombine(3,8,1,1,0);
  grAlphaTestFunction(7);
  grAlphaTestReferenceValue(0x80);
  grTexFilterMode(0,0,0);
  if (1 < DAT_105ccbd0) {
    grTexFilterMode(1,0,0);
  }
  grConstantColorValue(0xffffffff);
  grColorCombine(1,0,0,2,0);
  if (1 < DAT_105ccbd0) {
    grTexCombine(1, 1, 0, 1, 0, 0, 0);
    grTexCombine(0, 3, 8, 3, 8, 0, 0);
  }
  else {
    grTexCombine(0, 1, 0, 1, 0, 0, 0);
  }
  return;
}

extern unsigned int DAT_105d17ec;
extern int DAT_10661844;
extern char DAT_1066185c;
extern char DAT_10661860;
extern char DAT_10661864;
extern char DAT_10661868;
extern char DAT_1066186c;
extern char DAT_10661878;
extern char DAT_10661884;
extern char DAT_10661888;
extern char DAT_1066188c;
extern char DAT_10661890;
extern char DAT_10661904;
extern float _DAT_10077460;
void __stdcall grTexSource(int tmu, int start, int evenOdd, void *info);
void __stdcall grTexClampMode(int tmu, int s, int t);
void __stdcall grTexFilterMode(int tmu, int min, int mag);
void __stdcall grTexLodBiasValue(int tmu, float bias);
void __stdcall grTexMipMapMode(int tmu, int mode, int lodBlend);

/* WHAT IT DOES: bind one texture slot to the 3dfx card: sets its source,
 * clamping, filtering and level-of-detail bias from the stored table entry.
 * Silently does nothing if the index is past the high-water mark or the slot
 * is empty. */
/* @implements 0x10028420 glide FUN_10028420 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_10028420(unsigned int param_1)

{
  int uVar1;
  int iVar2;
  
  if ((param_1 < DAT_105d17ec) && (iVar2 = param_1 * 0xd8, (&DAT_10661844)[param_1 * 0x36] != 0)) {
    uVar1 = *(int *)(&DAT_10661884 + iVar2);
    grTexSource(uVar1,*(int *)(&DAT_1066188c + iVar2),*(int *)(&DAT_10661888 + iVar2),
                &DAT_10661904 + iVar2);
    grTexClampMode(uVar1,*(int *)(&DAT_10661868 + iVar2),
                   *(int *)(&DAT_1066186c + iVar2));
    grTexFilterMode(uVar1,*(int *)(&DAT_10661864 + iVar2),
                    *(int *)(&DAT_10661860 + iVar2));
    grTexLodBiasValue(uVar1,(float)*(unsigned int *)(&DAT_10661878 + iVar2) * _DAT_10077460);
    grTexMipMapMode(uVar1,*(int *)(&DAT_1066185c + iVar2),
                    *(int *)(&DAT_10661890 + iVar2));
  }
  return;
}


extern int DAT_105d17ec;

extern int DAT_10661834;
extern int DAT_10661844;
int __stdcall grTexMinAddress(int);

/* WHAT IT DOES: clear the whole texture table -- marks all 256 slots free
 * and resets the high-water mark, so the next frame's textures start from an
 * empty card. */
/* @implements 0x100281C0 glide FUN_100281c0 */
/* auto-filed from ghidra --refine; transforms: callconv */

void FUN_100281c0(void)

{
  int *puVar1;
  int uVar2;
  int iVar3;
  
  puVar1 = &DAT_10661844;
  iVar3 = 0;
  do {
    *puVar1 = 0;
    puVar1 = puVar1 + 0x36;
  } while ((int)puVar1 < 0x10697844);
  DAT_105d17ec = 0;
  puVar1 = &DAT_10661834;
  do {
    uVar2 = grTexMinAddress(iVar3);
    puVar1[-1] = uVar2;
    *puVar1 = 0x200000;
    puVar1 = puVar1 + 2;
    iVar3 = iVar3 + 1;
  } while ((int)puVar1 < 0x10661844);
  return;
}

#endif /* BR_MATCHING_BUILD */
