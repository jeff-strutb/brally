/* Ghidra-decompiled functions that match bit-exact but cannot live in their
 * named modules due to header/context conflicts, plus the hand-improved
 * near-miss WIP batch.  See comments on each. */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)

#include <windows.h>
#include "br_match.h"   /* BR_THISCALL1 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int FUN_10035400();
void BrUiSprClip();
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;
extern int DAT_10ac53e8;
extern int DAT_10ac5d84;
extern int g_aBrSndBankVoice;

/* WHAT IT DOES: get the desktop window and call into the display setup path. */
/* NOTE: context-sensitive codegen — matches here but not in br_drawcar.c. */
/* @implements 0x10009C00 glide BrDesktopSetup */

int BrDesktopSetup(void)

{
  GetDesktopWindow();
  FUN_10035400();
  return;
}

/* WHAT IT DOES: draw one clipped sprite glyph from the font sheet. */
/* NOTE: BrUiSprClip takes 7 args in the port header, 6 in the original. */
/* @implements 0x10058380 glide BrSprFontDraw */

int BrSprFontDraw(int param_1,int param_2,unsigned int param_3,int param_4,
                 int param_5)

{
  BrUiSprClip(DAT_10ac5d84,param_1,param_2,(&DAT_10ac53e8)[(param_3 & 0xffff) * 2],param_4,param_5)
  ;
  return;
}

/* WHAT IT DOES: set the pan value on a sound voice by bank index. */
/* NOTE: context-sensitive codegen — matches here but not in slice6_76.c. */
/* @implements 0x1006B5B0 glide BrSndVoiceSetPan */

int BrSndVoiceSetPan(int param_1,int param_2)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if ((&g_aBrSndBankVoice)[param_1] != 0) {
      *(int *)((&g_aBrSndBankVoice)[param_1] + 0x18) = param_2;
      return 1;
    }
    return 0;
  }
  return 1;
}

/* ==================================================================== */
/* Near-miss WIP batch from the automated pipeline pile, hand-improved  */
/* and audited 2026-08-24.  Nine functions below are tagged but still   */
/* diff; the three siblings that verified byte-exact were filed into    */
/* br_input.c and br_menuact.c.  Per-function blocker notes inline.     */
/* Open lead (NOT a proven idiom): the original of BrCdAudioTick emits  */
/* `and 0x3f; or 0x40` where (x & 0x7f) | 0x40 peepholes differently    */
/* for us -- bit-6 mask shrink, source spelling unknown.                */
/* ==================================================================== */


/* ------------------------------------------------------------------ */
/* 0x1002F282                                                         */
/* ------------------------------------------------------------------ */

typedef struct { void *p; } BrPtrArg;
void __fastcall FUN_100634b0(void *, BrPtrArg);
void FUN_1006c460(void);
void FUN_10072840(void);
void FUN_1006a320(void);
void FUN_10005cd0(void);
void FUN_1001cd50(void);
void FUN_10063970(int, int, int, int, int);
void FUN_1005a420(void);

extern int DAT_106ec760;
extern volatile int DAT_10b71a68;
extern int DAT_106e9a34;
extern volatile int DAT_10b71a6c;
extern int DAT_10b72f48;
extern int DAT_10b71290;
extern volatile int DAT_10226a48;
extern HANDLE DAT_106ed6e0;

/* WHAT IT DOES: tears down the current session (net, handles, video) and
 * brings the renderer back up at 640x480x16 if the clock pair drifted. */
/* @implements 0x1002F282 glide BrSessionReinitVideo */
void BrSessionReinitVideo(void)
{
    BrPtrArg a;

    if ((DAT_106ec760 != DAT_10b71a68) || (DAT_106e9a34 != DAT_10b71a6c)) {
        a.p = &DAT_10b72f48;
        FUN_100634b0(&DAT_10b71290, a);
    }
    FUN_1006c460();
    FUN_10072840();
    if (DAT_10226a48 != 0) {
        if (DAT_10226a48 > 1) {
            FUN_1006a320();
        }
        FUN_10005cd0();
    }
    FUN_1001cd50();
    CloseHandle(DAT_106ed6e0);
    DAT_106ed6e0 = 0;
    FUN_10063970(3, 0x280, 0x1e0, 0x10, 0);
    FUN_1005a420();
}

/* ------------------------------------------------------------------ */
/* 0x1006AFF0                                                         */
/* ------------------------------------------------------------------ */

typedef union { unsigned char b; unsigned int u; } BrU8Arg;
typedef union { unsigned short w; unsigned int u; } BrU16Arg;
typedef union { unsigned int u; } BrU24Arg;   /* same 4-byte stack slot */
int __fastcall FUN_1006d180(void *);
void __fastcall FUN_1006cfa0(void *, BrU8Arg);
void __fastcall FUN_1006cfc0(void *, BrU16Arg);
void __fastcall FUN_1006d000(void *, BrU24Arg);
extern unsigned char DAT_1021cdf8;
extern unsigned char DAT_100b3014;
extern unsigned char DAT_10226e80;
extern unsigned short DAT_1021ce50;
extern unsigned char DAT_1021cdb0;
extern unsigned char DAT_10226a40;
extern unsigned char DAT_10226a3c;

/* WHAT IT DOES: writes one race-options record into a net bitstream, but
 * only if nine more bytes still fit in the 256-byte buffer. */
/* WHAT IT DOES: appends one tagged field to an outgoing network packet -- a
 * tag byte carrying the field kind in its low bits, then a three-byte number
 * and a two-byte one. If the packet has no room for those six bytes it writes
 * nothing and reports failure, so the send thread can close the packet and
 * start another. Its sibling below writes the race-options field the same way
 * under a different tag. */
/* @implements 0x1006AFA0 glide BrNetWriteTagC0 */
/* RESIDUE: 2 instructions / 8 bytes, and the cause is a construct C cannot
 * spell. Everything else is exact (RAW and REGNORM 2+0, the two rows below).
 *
 * The original passes the tag byte as `mov al,[esp+0xc]; or al,0xc0; push eax`
 * -- eax pushed with its upper three bytes still holding the size check's
 * result. MSVC only leaves a stack argument dirty like that when the CALLEE'S
 * PARAMETER IS A BYTE TYPE, and a byte parameter is register-eligible, so
 * under __fastcall it takes edx instead of the stack. There is no C spelling
 * that puts a char-typed argument on the stack of a thiscall: every wrapper
 * homes the partial write first.
 *
 * PROBED AND DEAD, do not re-run: a 1-byte struct, a 4-byte union written
 * through its char member, and a 4-byte struct with three explicit pad bytes
 * -- all three emit the same `mov [slot],al; mov ecx,[slot]; push ecx`.
 *
 * The two reachable halves ARE fixed and are worth keeping: the guard is
 * written positively (`if (room) { ...; return 1; } return 0;`) so the two
 * exits land the original's way round, and the 16-bit argument is passed as a
 * FULL DWORD through the union's `.u` member because the original loads the
 * whole parameter slot -- a partial `.w` write homes the union and costs two
 * more instructions.
 *
 * ‼ This routes to the C++ TU lane. BrNetWriteRaceOpts below makes EIGHT of
 * these byte-writer calls and is at 81 diffs, so it inherits the same wall
 * eight times over; do not grind it in C either. */
int BrNetWriteTagC0(void *pThis, unsigned char kind, unsigned int a,
                    unsigned int b)
{
    BrU8Arg  t;
    BrU24Arg u;
    BrU16Arg w;

    if (FUN_1006d180(pThis) + 6 <= 0x100) {
        t.b = (unsigned char)(kind | 0xc0);
        FUN_1006cfa0(pThis, t);
        u.u = a;
        FUN_1006d000(pThis, u);
        /* `.u`, not `.w`: the original loads the whole dword out of the
         * parameter slot (`mov edx,[esp+0x14]`) and pushes it. A partial
         * write to `.w` makes MSVC home the union first, which is two extra
         * instructions. */
        w.u = b;
        FUN_1006cfc0(pThis, w);
        return 1;
    }
    return 0;
}

/* @implements 0x1006AFF0 glide BrNetWriteRaceOpts */
int BrNetWriteRaceOpts(void *pThis, unsigned char kind)
{
    BrU8Arg b;
    BrU16Arg w;

    if (FUN_1006d180(pThis) + 9 <= 0x100) {
        b.b = (unsigned char)(kind | 0xe0);
        FUN_1006cfa0(pThis, b);
        b.b = DAT_1021cdf8;
        FUN_1006cfa0(pThis, b);
        b.b = DAT_100b3014;
        FUN_1006cfa0(pThis, b);
        b.b = DAT_10226e80;
        FUN_1006cfa0(pThis, b);
        w.w = DAT_1021ce50;
        FUN_1006cfc0(pThis, w);
        b.b = DAT_1021cdb0;
        FUN_1006cfa0(pThis, b);
        b.b = DAT_10226a40;
        FUN_1006cfa0(pThis, b);
        b.b = DAT_10226a3c;
        FUN_1006cfa0(pThis, b);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 0x10038A80                                                         */
/* ------------------------------------------------------------------ */

extern int DAT_10ac5a48;
extern int DAT_10ac5a4c;

/* WHAT IT DOES: maps the current track/car-class menu selection onto the
 * two-byte letter id stored on the player record. */
/* @implements 0x10038A80 glide BrMenuSetTrackLetter */
int BrMenuSetTrackLetter(int param_1)
{
    int sel;
    int none;
    unsigned short *slot;

    sel = DAT_10ac5a48;
    slot = (unsigned short *)(param_1 + 0x1e20c);
    none = -1;
    if (sel > 0) {
        switch (sel) {
        case 2:
            *slot = 0x6d;
            break;
        case 3:
            *slot = 0x6e;
            break;
        case 4:
            *slot = 0x6c;
            break;
        default:
            *slot = (short)none;
            break;
        }
    }
    if (DAT_10ac5a48 == 0) {
        switch (DAT_10ac5a4c & 0xff) {
        case 1:
            *slot = 0x48;
            break;
        case 2:
            *slot = 0x4a;
            return 1;
        case 3:
            *slot = 0x4c;
            return 1;
        default:
            *slot = (short)none;
            return 1;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10008AB0                                                         */
/* ------------------------------------------------------------------ */

typedef struct { char *name; } BrPodNameArg;
typedef struct { void *p; } BrPodPtrArg;
typedef struct { int n; } BrPodLenArg;
void *__fastcall FUN_10008e10(void *, BrPodNameArg);
void __fastcall FUN_10008e60(void *, BrPodPtrArg, BrPodPtrArg, int);
void FUN_10008ec0(char *, char *);
void *FUN_10074572(unsigned int);
extern char DAT_1007b5bc;
extern char s__s_is_not_a_valid_POD_file_1007b5a0[];

/* WHAT IT DOES: opens the POD named on the object, checks the three-byte
 * magic, allocates 76 bytes per directory entry and reads the directory. */
/* @implements 0x10008AB0 glide BrPodOpen */
void __fastcall BrPodOpen(void *pThis)
{
    void *pIo;
    void *pFile;
    unsigned int cb;
    BrPodNameArg name;
    BrPodPtrArg file;
    BrPodPtrArg buf;

    name.name = (char *)pThis + 0x20;
    pIo = (char *)pThis + 4;
    pFile = FUN_10008e10(pIo, name);
    *(void **)((char *)pThis + 0x1c) = pFile;
    file.p = pFile;
    buf.p = (char *)pThis + 8;
    FUN_10008e60(pIo, file, buf, 0x10);
    if (strncmp((char *)pThis + 8, &DAT_1007b5bc, 3) != 0) {
        FUN_10008ec0(s__s_is_not_a_valid_POD_file_1007b5a0,
                     (char *)pThis + 0x20);
    }
    cb = *(int *)((char *)pThis + 0x10) * 0x4c;
    *(unsigned int *)((char *)pThis + 0x420) = cb;
    *(void **)((char *)pThis + 0x18) = FUN_10074572(cb);
    fseek(*(FILE **)((char *)pThis + 0x1c),
          *(long *)((char *)pThis + 0x14), 0);
    file.p = *(void **)((char *)pThis + 0x1c);
    buf.p = *(void **)((char *)pThis + 0x18);
    FUN_10008e60(pIo, file, buf, *(int *)((char *)pThis + 0x420));
}

/* ------------------------------------------------------------------ */
/* 0x10005400                                                         */
/* ------------------------------------------------------------------ */

int FUN_10004d80(int);
void FUN_10004ad0(void *, int, int, unsigned char, unsigned char,
                  unsigned char, int, void *, int, int);
extern HANDLE DAT_1021c90c;
extern int DAT_1021ce44;
extern int DAT_105ccb80;
extern int DAT_1007b264;
extern int DAT_10226e7c;
extern unsigned char DAT_10af3bb4;
extern unsigned char DAT_10af3bb5;
extern unsigned char DAT_10af3bb6;
extern int DAT_10273330;
extern int DAT_10273328;
extern int DAT_10b71648;

/* WHAT IT DOES: under the CD-audio mutex, ticks a 100-step counter and
 * refreshes the on-screen time string when the counter is live. */
/* @implements 0x10005400 glide BrCdAudioTick */
void BrCdAudioTick(void)
{
    int n;
    int flags;

    WaitForSingleObject(DAT_1021c90c, 0xffffffff);
    n = DAT_1021ce44;
    if (n != 0) {
        n++;
        DAT_1021ce44 = n;
        if (n >= 0x64) {
            DAT_105ccb80 = 1;
            n = 0;
            DAT_1021ce44 = 0;
        }
    }
    ReleaseMutex(DAT_1021c90c);
    if (n != 0) {
        flags = FUN_10004d80(DAT_1007b264);
        flags &= 0x7f;
        flags |= 0x40;
        FUN_10004ad0(&DAT_10273328, DAT_1007b264, DAT_10226e7c,
                     DAT_10af3bb4, DAT_10af3bb5, DAT_10af3bb6,
                     DAT_10273330, &DAT_10b71648, flags, 0);
    }
}

/* ------------------------------------------------------------------ */
/* 0x10029CD0                                                         */
/* ------------------------------------------------------------------ */

void FUN_1006e1a0(void);
extern unsigned int DAT_10697a58;
extern int DAT_10697a5c;
extern int DAT_106b7aa0;

/* WHAT IT DOES: frees every per-entry graphics pointer in the entity table,
 * then frees the table itself and zeros the counts. */
/* @implements 0x10029CD0 glide BrEntGfxFreeAll */
void BrEntGfxFreeAll(void)
{
    unsigned int i;
    int off;
    char *base;
    int slot;
    int k;
    void *p;

    k = 4;
    FUN_1006e1a0();
    i = 0;
    off = 0;
    base = (char *)DAT_106b7aa0;
    if (DAT_10697a58 > 0) {
        do {
            if (*(int *)(base + off + 0x26c) != 0) {
                slot = off + 0x280;
                k = 4;
                do {
                    p = *(void **)(base + slot);
                    if (p != 0) {
                        free(p);
                        *(int *)(DAT_106b7aa0 + slot) = 0;
                        base = (char *)DAT_106b7aa0;
                    }
                    slot += 4;
                    k--;
                } while (k != 0);
            }
            i++;
            off += 0x2b4;
        } while (i < DAT_10697a58);
    }
    DAT_10697a58 = 0;
    DAT_10697a5c = 0;
    free(base);
    DAT_106b7aa0 = 0;
}

/* ------------------------------------------------------------------ */
/* 0x10013F20                                                         */
/* ------------------------------------------------------------------ */

extern int DAT_10396f10;
extern int DAT_10396f48;
extern int DAT_104ab4e8;
extern int DAT_104ab4ec;
extern int DAT_104ab500;

/* WHAT IT DOES: picks the unused sound-bank slot with the lowest use count
 * (skipping the one currently playing) and rotates the last/current pair. */
/* @implements 0x10013F20 glide BrSndBankPickSlot */
void BrSndBankPickSlot(void)
{
    int chosen;
    unsigned int best;
    int i;
    unsigned int *cost;
    int *flag;
    int cur;
    int prev;

    chosen = -1;
    best = 0xffffffffu;
    i = 0;
    cost = (unsigned int *)&DAT_10396f48;
    flag = &DAT_10396f10;
    cur = DAT_104ab4e8;
    do {
        if (*flag == 0 && i != cur && *cost <= best) {
            chosen = i;
            best = *cost;
        }
        flag++;
        i++;
        cost += 0xb83c;
    } while ((int)flag < 0x10396f24);
    if (cur < 0) {
        prev = 0;
    } else {
        prev = *(int *)((char *)&DAT_10396f48 + cur * 0x2e0f0);
    }
    DAT_104ab500 = DAT_104ab4ec;
    DAT_104ab4ec = cur;
    DAT_104ab4e8 = chosen;
    *(int *)((char *)&DAT_10396f48 + chosen * 0x2e0f0) = prev + 1;
}

/* ------------------------------------------------------------------ */
/* 0x10036E50                                                         */
/* ------------------------------------------------------------------ */

typedef struct BrIUnk BrIUnk;
struct BrIUnk {
    struct {
        int (__stdcall *QueryInterface)(BrIUnk *, void *, void **);
        int (__stdcall *AddRef)(BrIUnk *);
        int (__stdcall *Release)(BrIUnk *);
    } *vt;
};
int __stdcall FUN_10072960(int, BrIUnk **, int, int, int);
void FUN_10036f40(int, BrIUnk *);
extern int DAT_100788e8;
extern int DAT_105bc72c;

/* WHAT IT DOES: creates a DirectPlay object, queries the wanted interface
 * and hands it back, releasing the original on success or both on failure. */
/* @implements 0x10036E50 glide BrDpCreateIface */
int BrDpCreateIface(BrIUnk **out)
{
    BrIUnk *a;
    BrIUnk *b;
    int hr;

    a = 0;
    b = 0;
    hr = FUN_10072960(0, &a, 0, 0, 0);
    if (hr >= 0) {
        hr = a->vt->QueryInterface(a, &DAT_100788e8, (void **)&b);
        if (hr < 0) {
            goto fail;
        }
        a->vt->Release(a);
        a = 0;
        FUN_10036f40(DAT_105bc72c, b);
        *out = b;
        return 0;
    }
fail:
    if (a != 0) {
        a->vt->Release(a);
    }
    if (b != 0) {
        b->vt->Release(b);
    }
    return hr;
}

/* ------------------------------------------------------------------ */
/* 0x1003FBE0                                                         */
/* ------------------------------------------------------------------ */

typedef struct BrObjVt {
    void (__fastcall *fn[8])(void *);
} BrObjVt;
typedef struct BrObj {
    BrObjVt *vt;
} BrObj;
typedef struct { int v; } BrIntArg;
typedef struct BrDelVt {
    void (__fastcall *del)(void *, BrIntArg);
} BrDelVt;
typedef struct BrDelObj {
    BrDelVt *vt;
} BrDelObj;
extern BrDelObj *DAT_10ac5c5c;
extern int DAT_10ac5c80;
extern int DAT_10ac5d18;
extern int DAT_10ac5d24;
extern int DAT_10ac5c3c;
extern int DAT_100aab94;
extern char DAT_10ac5870;
extern char DAT_10ac46a0;
extern char DAT_10396f08;
extern int DAT_10ac5c84;

/* WHAT IT DOES: shuts down the current menu object, clears the track-name
 * working buffers to the default string and restores the previous object. */
/* port-only body; Glide match is src/core/cpp/0x1003FBE0.cpp */
void BrMenuResetTrackStr(int param_1)
{
    BrObj *obj;
    BrDelObj *p;
    BrIntArg one;

    obj = *(BrObj **)(param_1 + 0x2ae8);
    obj->vt->fn[7](obj);
    p = DAT_10ac5c5c;
    if (p != 0) {
        one.v = 1;
        ((void (__fastcall *)(void *, BrIntArg))p->vt->del)(p, one);
    }
    DAT_10ac5c80 = 0;
    DAT_10ac5d18 = 0;
    DAT_10ac5d24 = 0;
    DAT_10ac5c3c = 0;
    DAT_100aab94 = -1;
    strcpy(&DAT_10ac5870, &DAT_10396f08);
    strcpy(&DAT_10ac46a0, &DAT_10396f08);
    DAT_10ac5c5c = (BrDelObj *)DAT_10ac5c84;
}

/* ------------------------------------------------------------------ */
/* 0x1003AF30  SetStatusText                                          */
/* ------------------------------------------------------------------ */

/* thiscall with 4 stack args.  __fastcall puts `this` in ecx; the second
 * register-eligible arg is edx.  Passing param_1 (already live in edx as
 * the push temp) rather than literal 0 avoids `xor edx,edx`. */
typedef int (__fastcall *BrCtlF34)(void *this, int _edx_unused, int, int, int, int);
extern int DAT_10ac4c58;
extern int DAT_100aacf8;

/* WHAT IT DOES: SetStatusText.  Looks up the control named by the root
 * page's status-line index (0x10AC4C58) on the current phase's first page
 * (0x10AC5C5C)->+0x14, and if that slot is occupied calls vtable +0x34
 * with (text, 1, 1, style 0x100AACF8). */
/* @implements 0x1003AF30 glide BrExt_100419D0 */
void BrExt_100419D0(int param_1)
{
    int *piVar1;

    piVar1 = *(int **)(*(int *)((int)DAT_10ac5c5c + 0x14) + 0x18 + DAT_10ac4c58 * 4);
    if (piVar1 != (int *)0x0) {
        (*(BrCtlF34 *)(*(int *)(piVar1) + 52))(piVar1, param_1, param_1, 1, 1, &DAT_100aacf8);
    }
    return;
}

/* ==================================================================== */
/* Isolated byte-exact matches harvested from bulk pipeline worktrees.   */
/* Verified match in a standalone TU; the port already carries           */
/* different-signature or different-context versions of several (the     */
/* file-exists helper, the flat-triangle commands), so they live here    */
/* rather than in their named modules -- same reason as the block above. */
/* ==================================================================== */

extern int DAT_1021c810;
extern char DAT_1007b1d4;
extern char DAT_1007b0e0;

/* WHAT IT DOES: reports whether a file can be opened for reading, and when
 * the verbose-debug flag is set also prints CHK_FileExists(path) to the
 * debugger. */
/* @implements 0x10003680 glide BrChkFileExists */
int BrChkFileExists(char *param_1)
{
    FILE *_File;
    char local_400[1024];

    if (DAT_1021c810 != 0) {
        sprintf(local_400, &DAT_1007b1d4, param_1);
        OutputDebugStringA(local_400);
    }
    _File = fopen(param_1, &DAT_1007b0e0);
    if (_File == (FILE *)0x0) {
        return 0;
    }
    fclose(_File);
    return 1;
}

void FUN_1001ff60(int, int, int);
void FUN_10020460(int, int, int);

/* WHAT IT DOES: draws one flat-shaded z-buffered triangle, permuting the
 * three vertex bytes according to a selector in the command. */
/* @implements 0x1001FEF0 glide BrDlCmdTri1FlatZ */
unsigned char *BrDlCmdTri1FlatZ(unsigned char *p)
{
    switch (p[7]) {
    case 0:
        FUN_1001ff60(p[6], p[5], p[4]);
        return p + 8;
    case 1:
        FUN_1001ff60(p[5], p[4], p[6]);
        return p + 8;
    default:
        FUN_1001ff60(p[4], p[6], p[5]);
        return p + 8;
    }
}

/* WHAT IT DOES: draws one flat-shaded triangle with the z-buffer off,
 * permuting the three vertex bytes according to a selector in the command. */
/* @implements 0x100203F0 glide BrDlCmdTri1Flat */
unsigned char *BrDlCmdTri1Flat(unsigned char *p)
{
    switch (p[7]) {
    case 0:
        FUN_10020460(p[6], p[5], p[4]);
        return p + 8;
    case 1:
        FUN_10020460(p[5], p[4], p[6]);
        return p + 8;
    default:
        FUN_10020460(p[4], p[6], p[5]);
        return p + 8;
    }
}

/* WHAT IT DOES: draws two flat-shaded z-buffered triangles from one
 * command, vertex bytes 0..2 then 4..6. */
/* @implements 0x10020CF0 glide BrDlCmdTri2FlatZ */
unsigned char *BrDlCmdTri2FlatZ(unsigned char *p)
{
    FUN_1001ff60(p[2], p[1], p[0]);
    FUN_1001ff60(p[6], p[5], p[4]);
    return p + 8;
}

/* WHAT IT DOES: draws two flat-shaded triangles with the z-buffer off,
 * vertex bytes 0..2 then 4..6. */
/* @implements 0x10020D30 glide BrDlCmdTri2Flat */
unsigned char *BrDlCmdTri2Flat(unsigned char *p)
{
    FUN_10020460(p[2], p[1], p[0]);
    FUN_10020460(p[6], p[5], p[4]);
    return p + 8;
}

extern int DAT_10ac5c50;
extern int DAT_100a9360;
extern int DAT_10ac5bf4;
extern unsigned short DAT_10ac5b3a;
extern int DAT_10ac40a0;
extern int DAT_10ac5c54;
extern int DAT_100aab8c;

/* WHAT IT DOES: reports whether a numbered input bit is set, forcing
 * off for code 12 and on for 13/14 under a lock flag. */
/* @implements 0x100387F0 glide BrInputBitHeld */
int BrInputBitHeld(int code)
{
    if (code == 0xc)
        return 0;
    if (DAT_10ac5c50 != 0)
        return 1;
    if (DAT_100a9360 == 0) {
        if (DAT_10ac5bf4 != 0)
            return (1 << code) & DAT_10ac5b3a;
        return (1 << code) & DAT_10ac40a0;
    }
    if ((DAT_10ac5c54 != 0) && ((code == 0xe) || (code == 0xd)))
        return 1;
    return (1 << code) & DAT_100aab8c;
}

extern int DAT_10ac5bec;
int BR_THISCALL1 FUN_1006d180(void *pThis);
int BR_THISCALL1 FUN_1006d190(void *pThis);
int FUN_1002f790(int *p, int a, int b, int c, int d);
int FUN_10009a00(int a, int b, int c, int d, int e, int f);

/* WHAT IT DOES: sends a DirectPlay payload built from a counted state
 * object, or bails out with that count when a skip flag is set. */
/* @implements 0x10005140 glide BrNetTrySend */
int BrNetTrySend(int *param_1, void *param_2)
{
    int n;

    if (DAT_10ac5bec != 0)
        return FUN_1006d180(param_2);
    if (param_1[3] != 0)
        FUN_1002f790(param_1,
                     FUN_1006d190(param_2),
                     FUN_1006d180(param_2),
                     1, 1);
    n = FUN_10009a00(*param_1, param_1[2], 0, 0,
                     FUN_1006d190(param_2),
                     FUN_1006d180(param_2));
    if (n == 0)
        return FUN_1006d180(param_2);
    return -1;
}

int BR_THISCALL1 BrCountedTotal(void *);
int BR_THISCALL1 BrStateGetField10(void *);

/* WHAT IT DOES: if a net-lock flag is set, just return the counted
 * total; otherwise either dispatch through the local path or send a
 * DirectPlay payload built from the link and the counted object. */
/* @implements 0x10004A40 glide BrCountedNetSend */
int BrCountedNetSend(int *param_1, void *param_2)
{
    int iVar2;

    if (DAT_10ac5bec != 0) {
        return BrCountedTotal(param_2);
    }
    if (param_1[3] != 0) {
        FUN_1002f790(param_1,
                     BrStateGetField10(param_2),
                     BrCountedTotal(param_2),
                     1, 1);
        return BrCountedTotal(param_2);
    }
    iVar2 = FUN_10009a00(*param_1, param_1[2], 1, 0,
                         BrStateGetField10(param_2),
                         BrCountedTotal(param_2));
    if (iVar2 == 0) {
        return BrCountedTotal(param_2);
    }
    return -1;
}

#endif /* BR_MATCHING_BUILD */
