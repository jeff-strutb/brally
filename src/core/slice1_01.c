/* slice1_01.c -- BRD3D.dll 0x10001000-0x10004910, a later pass. See slice1_01.h. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_01.h"

#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * 0x10001000 -- zlib adler32.
 *
 * Identified by the two magic constants the original carries verbatim:
 * 0x15B0 (NMAX = 5552) as the outer chunk cap and 0xFFF1 (BASE = 65521) as
 * the modulus, plus the DO16 unrolled body at 0x1000104E..0x100010E4.
 *
 * Argument order is the zlib one, traced through the prologue: after
 * `push esi` / `push edi` the reads at [esp+0xC] land on arg2 (buf) and arg1
 * (adler) respectively, and after `push ebx` the read at [esp+0x18] lands on
 * arg3 (len).
 *
 * The len == 0 path is reached by `test ebx,ebx / jbe`; after a `test` the
 * carry flag is clear, so jbe is just je -- it is an equality test, not the
 * signed/unsigned comparison it looks like.
 */
unsigned long BrAdler32(unsigned long adler, const unsigned char *pBuf,
                        unsigned int len)
{
    unsigned long s1 = adler & 0xFFFFuL;
    unsigned long s2 = (adler >> 16) & 0xFFFFuL;
    unsigned int  k;

    if (pBuf == NULL) {
        return 1uL;
    }

    while (len > 0u) {
        k = (len < 5552u) ? len : 5552u;
        len -= k;

        /* DO16, then the remainder one byte at a time. */
        while (k >= 16u) {
            unsigned int i;
            for (i = 0u; i < 16u; ++i) {
                s1 += *pBuf++;
                s2 += s1;
            }
            k -= 16u;
        }
        while (k > 0u) {
            s1 += *pBuf++;
            s2 += s1;
            --k;
        }

        s1 %= 65521uL;
        s2 %= 65521uL;
    }

    return (s2 << 16) | s1;
}

/* ---------------------------------------------------------------------------
 * 0x10002A20 (partial) -- 8-bit volume to the backend's 0..10000 scale.
 *
 * The original builds 10000*v with four `lea [r+r*4]` (x5 each, so x625) and
 * a `shl 4`, then divides by 255 with the signed magic 0x80808081 / sar 7 /
 * sign fixup. v is masked to 8 bits before any of that, so the input is
 * always non-negative and the sign fixup never fires.
 */
/* WHAT IT DOES: converts a music volume from the 0-255 scale the game's own
 * settings use into the 0-10000 scale Windows CD audio wants. A volume of
 * exactly 256 comes out as silence rather than full, because only the low
 * byte is looked at. */
/* BrCdVolumeScale moved below the CD-globals declarations (it references the
 * three CD-enable guards and the g_575454 backend pointer). */

/* ---------------------------------------------------------------------------
 * 0x10002DE0 -- 64x64 u16 grid sample.
 *
 * The four guards are `fcomp` against 0.0f (0x1008F09C) and 2048.0f
 * (0x1008F0A0), read back with fnstsw / `test ah,1`, i.e. the C0 bit, i.e.
 * "ST < operand". Order in the original is x>=0, x<2048, y>=0, y<2048; x is
 * the one loaded twice through [esp+8] once esi has been pushed.
 *
 * The scale is 0x1008F0A4 = 0.03125f = 1/32, exact in binary, so no rounding
 * question arises before the truncation. 0x1007C8A0 is __ftol (it sets the
 * x87 rounding field to 0xC00 = toward zero and does `fistp qword`).
 *
 * Only AL of each conversion is consumed (`movzx si, al` / `movzx ax, al`),
 * so a value of 256 or more would wrap -- unreachable given the 2048 guard,
 * but reproduced with the mask below rather than assumed away.
 *
 * DEVIATION: the grid base was the global at 0x106C7C6C; it is a parameter.
 */
/* WHAT IT DOES: looks up a place in the world on a coarse 64-by-64 grid --
 * each square covering thirty-two world units, so the grid spans a square
 * region a couple of thousand units across -- and hands back both that
 * square's value and the difference to the square next along, so a caller can
 * blend between the two. WHAT THE GRID HOLDS IS NOT ESTABLISHED HERE. A
 * position outside the covered region answers zero, which is indistinguishable
 * from a square whose value genuinely is zero. */
/* @implements 0x10002DE0 d3d BrGrid64Sample */
uint32_t BrGrid64Sample(const uint16_t *pGrid, float x, float y)
{
    unsigned int col, row, idx;
    uint32_t t0, t1, acc;

    /* Written as negated comparisons so NaN takes the reject path, which is
     * what the original does: fcomp with a NaN sets C0, and the first guard
     * rejects on C0. */
    if (!(x >= 0.0f))    { return 0u; }
    if (!(x < 2048.0f))  { return 0u; }
    if (!(y >= 0.0f))    { return 0u; }
    if (!(y < 2048.0f))  { return 0u; }

    col = (unsigned int)(long)(x * 0.03125f) & 0xFFu;
    row = (unsigned int)(long)(y * 0.03125f) & 0xFFu;

    /* esi holds row<<6 with the caller's leftover high bits still in it and
     * eax holds __ftol's high bits; both are discarded by `and 0xffff`. */
    idx = ((row << 6) + col) & 0xFFFFu;

    t0 = pGrid[idx];
    t1 = pGrid[(idx + 1u) & 0xFFFFu];

    /* Literally `t1 + t0*65535`, shifted up 16 -- the low 16 bits of that sum
     * are (t1 - t0) mod 65536, which is the per-cell step. */
    acc = (uint32_t)((t1 + t0 * 65535u) << 16);
    return acc | t0;
}

extern int BrGetTimerState(void);
extern int DAT_1021c908;


/* ---------------------------------------------------------------------------
 * The CHK_* wrappers.
 *
 * DEVIATION (all five): the original formats into a 0x400-byte stack buffer
 * with sprintf and ships it to OutputDebugStringA. Here the message goes
 * straight to stderr -- no fixed buffer, so the %s cases cannot overflow it,
 * which the original could.
 */

int BrChkVerbose = 0;   /* 0x10220CE0 */

/* 0x100030E0  FCHK_FRead.
 *
 * GOTCHA: the fourth argument is a FILE **. The original does
 * `mov ecx, [eax]` on it before pushing it to fread.
 *
 * The byte count is a 32-bit `imul`, so it wraps; kept in uint32_t so a
 * wrapping size*count still produces the original's early-out and the
 * original's message.
 */
/* WHAT IT DOES: reads from a file and insists on getting everything asked
 * for. Reading nothing at all is reported to the caller as a plain failure --
 * that is how the game detects the end of a file -- but a short read, where
 * some but not all of the data arrived, is treated as the file being damaged
 * and kills the game with a message. */
/* @implements 0x100030E0 d3d BrFChkFRead */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif
/* RESIDUE (8 masked diffs, T3a, REGNORM 0+0): the original homes `size` in
 * ebx and `count` in edi; this build homes them the other way round, which
 * flips the two `push`es and the `imul` operands. Every instruction is the
 * original's. Writing the product `count * size` instead of `size * count`
 * changes nothing -- VC5 canonicalises the multiply the same way it does a
 * commutative add. */
int BrFChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile)
{
#ifdef BR_MATCHING_BUILD
    /* The original formats the failure message into a 0x400-byte stack buffer
     * (allocated in the prologue) and ships it to OutputDebugStringA. */
    char buf[0x400];
#endif
    uint32_t wanted = (uint32_t)size * (uint32_t)count;
    size_t   got;

    if (wanted == 0u) {
        return 1;
    }

    got = fread(pDst, size, count, *ppFile);

    if (got == 0u) {
        return 0;
    }
    if (got == count) {
        return 1;
    }

#ifdef BR_MATCHING_BUILD
    wsprintfA(buf,
              "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
              (int)wanted, (int)((uint32_t)got * (uint32_t)size));
    OutputDebugStringA(buf);
    exit(1);
#else
    fprintf(stderr,
            "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
            (int)wanted, (int)((uint32_t)got * (uint32_t)size));
    exit(1);
#endif

    return 1;   /* the original falls through to the `mov eax,1` tail */
}

/* 0x10003170  CHK_FRead. Returns the destination buffer. */
/* WHAT IT DOES: the same read, for callers who cannot cope with the file
 * ending -- hitting the end of the file kills the game rather than being
 * reported back. Used where the data being read is required for the game to
 * carry on at all. */
/* port-only body; Glide match is src/core/generated/0x100034C0.c */
void *BrChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile)
{
    if (BrFChkFRead(pDst, size, count, ppFile) == 0) {
        fprintf(stderr,
                "CHK_FRead(): trying to read %u bytes, but got EOF.\n",
                (unsigned int)((uint32_t)count * (uint32_t)size));
        exit(1);
    }
    return pDst;
}

/* 0x10003320  CHK_FileExists. */
int BrChkFileExists(const char *pPath)
{
    FILE *pFile;

    if (BrChkVerbose != 0) {
        fprintf(stderr, "CHK_FileExists(%s)\n", pPath);
    }

    /* The mode string is the literal "rb" at 0x10094110; the original goes
     * through _fsopen with _SH_DENYNO, which plain fopen matches closely
     * enough on a single-process port. */
    pFile = fopen(pPath, "rb");
    if (pFile == NULL) {
        return 0;
    }
    fclose(pFile);
    return 1;
}

/* 0x10003390  CHK_AllocateMemory. */
/* WHAT IT DOES: asks for memory and gives up on the whole game if there is
 * none, naming what it was trying to make room for so the player sees which
 * part of the loading failed. Asking for nothing quietly gets nothing back. */
/* port-only body; Glide match is src/core/generated/0x100036F0.c */
/* @n64 0x8021A9B4 located */
void *BrChkAlloc(size_t size, const char *pWhat)
{
    void *pMem;

    if (size == 0u) {
        /* The original returns without touching EAX, which still holds the
         * zero size -- so a NULL return, not an allocation of one byte. */
        return NULL;
    }

    pMem = malloc(size);
    if (pMem == NULL) {
        fprintf(stderr,
                "CHK_AllocateMemory(): Out of memory: couldn't allocate %s\n",
                pWhat);
        exit(1);
    }
    return pMem;
}

/* 0x100033F0  CHK_ReAllocateMemory. */
/* WHAT IT DOES: grows or shrinks a block of memory, again giving up on the
 * whole game if that cannot be done. Asking for a size of nothing loses the
 * block that was just handed back, which is a leak in the original and is
 * preserved. */
/* port-only body; Glide match is src/core/generated/0x10003760.c */
void *BrChkRealloc(void *pMem, size_t size, const char *pWhat)
{
    void *pNew;

    /* The call comes FIRST; the size check is at 0x1000340F, after it. */
    pNew = realloc(pMem, size);

    if (size == 0u) {
        /* BUG PRESERVED: with pMem == NULL this has just performed a
         * zero-size allocation whose result is now dropped on the floor. */
        return NULL;
    }

    if (pNew == NULL) {
        fprintf(stderr,
                "CHK_ReAllocateMemory(): Out of memory: couldn't reallocate %s\n",
                pWhat);
        exit(1);
    }
    return pNew;
}

#ifdef _MSC_VER
typedef int (__stdcall *BrEarShutdownChannelFn)(int);
typedef int (__stdcall *BrCdVolumeSetFn)(int, int);
#else
typedef int (*BrEarShutdownChannelFn)(int);
typedef int (*BrCdVolumeSetFn)(int, int);
#endif

#ifdef BR_MATCHING_BUILD
extern int g_0940A4;
extern int g_220CD0;
extern int g_220C3C;
extern int g_220C40;
extern int g_220CD8;
extern int g_0940A8;
extern BrEarShutdownChannelFn g_575470;
extern BrCdVolumeSetFn g_575454;
__declspec(dllimport) unsigned long __stdcall mciSendCommandA(
    unsigned long id, unsigned long msg,
    unsigned long flags, unsigned long param);
#else
int g_0940A4;
int g_220CD0;
int g_220C3C;
int g_220C40;
int g_220CD8;
int g_0940A8;
BrEarShutdownChannelFn g_575470;
BrCdVolumeSetFn g_575454;
#endif

/* WHAT IT DOES: closes the EAR music channel if one is actually running. */
/* @implements 0x10002440 d3d BrCdMaybeClose */
int BrCdMaybeClose(void)
{
    if (g_0940A4 != 0) {
        if (g_220CD0 != 0) {
            if (g_220C3C != 0) {
                int h = g_0940A8;
                g_220C3C = 0;
#ifdef BR_MATCHING_BUILD
                return g_575470(h);
#else
                (void)h;
                return 1;
#endif
            }
        }
    }
    return 1;
}

/* WHAT IT DOES: converts a music volume from the 0-255 scale the game's own
 * settings use into the 0-10000 scale Windows CD audio wants, then hands it to
 * the backend -- but only when disc music is actually active (the same three
 * guards as BrCdMaybeClose). A volume of exactly 256 comes out as silence
 * rather than full, because only the low byte is looked at. */
/* @implements 0x10002A20 d3d BrCdVolumeScale */
int BrCdVolumeScale(int vol)
{
    if (g_0940A4 != 0) {
        if (g_220CD0 != 0) {
            if (g_220C3C != 0) {
#ifdef BR_MATCHING_BUILD
                g_575454(g_0940A8, (10000 * (vol & 0xFF)) / 255);
#else
                (void)vol;
#endif
            }
        }
    }
    return 1;
}

#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: pauses CD soundtrack if disc music is in use. */
/* @implements 0x10002AE0 d3d BrCdMciPause */
int BrCdMciPause(void)
{
    if (g_0940A4) {
        if (g_220CD0) {
            int media = g_220C3C;
            g_220CD8 = 1;
            if (media) {
                if (mciSendCommandA((unsigned long)g_220C40, 0x809u, 0, 0)) {
                    mciSendCommandA((unsigned long)g_220C40, 0x804u, 0, 0);
                    return 0;
                }
            }
        }
    }
    return 1;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int FUN_10002580();
extern int DAT_1021c778;
int BrSub10075020();
int BrWindowEarStartup();
extern int DAT_1021c77c;
extern char DAT_1021c80c;
__declspec(dllimport) int __stdcall PostMessageA(int hWnd, unsigned int msg, unsigned int wParam, int lParam);
extern int g_brCdTrackLast;
int FUN_100027e0();
int FUN_10002c50();
int BrCdEnableApply(char param_1);
extern int g_brCdEnabled;
extern int g_brCdMediaOk;
extern int g_brCdPlaying;
extern int g_brCdTrackCur;
extern int g_brCdTrackFirst;

/* WHAT IT DOES: get the current track number — EAR path when CD audio is enabled, real CD otherwise. */
/* @implements 0x10002C50 glide BrCdTrackGet */

int BrCdTrackGet(void)

{
  if (g_brCdEnabled == 1) {
    FUN_100027e0();
    return;
  }
  BrCdTrackGetEar();
  return;
}


/* WHAT IT DOES: set music volume — dispatches to EAR mixer or CD-audio path. */
/* @implements 0x10002D30 glide BrCdVolumeSet */

int BrCdVolumeSet(int param_1)

{
  if (g_brCdEnabled == 1) {
    BrCdEnableApply(param_1);
    return;
  }
  BrCdVolumeScale(param_1);
  return;
}

/* WHAT IT DOES: resume the current CD track if the disc is ready and playback is enabled. */
/* @implements 0x10002E80 glide BrCdTrackResume */

int BrCdTrackResume(void)

{
  int uVar1;
  
  if (((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) && (g_brCdMediaOk != 0)) {
    uVar1 = BrCdTrackPlay(g_brCdTrackCur);
    return uVar1;
  }
  return 1;
}

/* ‼ MAP DEFECT, and it is what blocks these two.  config/functions_glide.csv
 * lists 0x10002EB0 and 0x10002F10 as 86 bytes each.  They are not: each is a
 * 14-byte DISPATCHER followed by 16-byte alignment padding and then a
 * SEPARATE function that only the dispatcher reaches, by tail jump.
 *
 *     10002EB0  cmp dword ptr [g_brCdEnabled],1
 *     10002EB7  jne  10002EBE
 *     10002EB9  jmp  10002E20        <- tail call, a MAPPED function
 *     10002EBE  jmp  10002ED0        <- tail call, NOT in the map
 *     10002EC3  13 x nop             <- aligning 10002ED0 to 16
 *     10002ED0  the message-transport body, 54 bytes, ending in its own ret
 *
 * 0x10002ED0 and 0x10002F30 are both 16-byte aligned, which is a function
 * ENTRY, and the map had no row for either -- it merged each into the
 * dispatcher above it because nothing CALLS them, only jumps.  Two C
 * functions cannot be one symbol, so no spelling of a single 86-byte
 * function could ever have matched.  FIXED 2026-09-03: config/functions_glide.csv
 * now carries 0x10002EB0/32, 0x10002ED0/54, 0x10002F10/32, 0x10002F30/54 and
 * build/match/orig/ was re-extracted for the four.  All four are byte-exact.
 * The dispatcher's 32 bytes INCLUDE the 13 alignment nops, which MSVC emits
 * inside the first function, not the second.
 *
 * ‼ THE TELL, and it generalises.  Written inline instead -- one function
 * containing both arms -- VC5 hoists the `g_brCdEnabled` load into a
 * register and turns the original's `cmp dword ptr [g],1` into
 * `mov eax,[g] / cmp eax,1`, then re-uses eax for the second test where the
 * original re-reads the global.  That CSE was a SYMPTOM of the wrong
 * function boundary, not a defect of its own: two functions cannot share a
 * register, so **a global that the original re-reads across what looks like
 * a plain branch is evidence that the branch is a FUNCTION boundary.**
 * Together with an unconditional `jmp` followed by nops up to a 16-byte
 * address, that is the signature of a merged map row.
 *
 * 0x104B162C is the message-transport entry point and the second argument is
 * the command (4 = pause, 0xC = resume).  Both bodies normalise the result to
 * 0/1 with the original's `neg/sbb/neg`, which is what `!= 0` compiles to. */
extern int g_br0940A8;                              /* 0x1007B078 */
extern int (__stdcall *DAT_104b162c)(int, int);     /* 0x104B162C */

/* WHAT IT DOES: tell the CD drive to hold the music where it is, and report
 * whether it agreed.  With the disc missing or nothing playing there is
 * nothing to do and it reports success. */
/* @implements 0x10002ED0 glide BrCdPauseMsg */
static int BrCdPauseMsg(void)
{
  if (((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) && (g_brCdMediaOk != 0)) {
    return (*DAT_104b162c)(g_br0940A8,4) != 0;
  }
  return 1;
}

/* WHAT IT DOES: stop the music where it is, so it can be picked up again from
 * the same place.  If the game is playing its own music files it hands the job
 * to that path; otherwise it goes out to the CD drive. */
/* @implements 0x10002EB0 glide BrCdPause */
int BrCdPause(void)
{
  if (g_brCdEnabled == 1) {
    return BrCdMciPause();
  }
  return BrCdPauseMsg();
}

/* WHAT IT DOES: tell the CD drive to start playing again from where it was
 * paused, and report whether it agreed.  The twin of BrCdPauseMsg above,
 * differing only in the command it sends. */
/* @implements 0x10002F30 glide BrCdResumeMsg */
static int BrCdResumeMsg(void)
{
  if (((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) && (g_brCdMediaOk != 0)) {
    return (*DAT_104b162c)(g_br0940A8,0xc) != 0;
  }
  return 1;
}

/* WHAT IT DOES: start the music again from wherever it was paused. */
/* @implements 0x10002F10 glide BrCdResume */
int BrCdResume(void)
{
  if (g_brCdEnabled == 1) {
    return BrCdTrackResume();
  }
  return BrCdResumeMsg();
}

/* WHAT IT DOES: play the next CD track, clamping to the last track. */
/* @implements 0x10002CB0 glide BrCdTrackNext */

int BrCdTrackNext(void)

{
  int iVar1;
  
  if ((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) {
    iVar1 = BrCdTrackGet();
    g_brCdTrackCur = iVar1 + 1;
    if (iVar1 + 1 > g_brCdTrackLast) {
      g_brCdTrackCur = g_brCdTrackLast;
    }
    BrCdTrackPlay(g_brCdTrackCur);
  }
  return 1;
}

/* WHAT IT DOES: play the next CD track, wrapping to the first track past the last. */
/* @implements 0x10002CF0 glide BrCdTrackNextWrap */

int BrCdTrackNextWrap(void)

{
  int iVar1;
  
  if ((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) {
    iVar1 = BrCdTrackGet();
    g_brCdTrackCur = iVar1 + 1;
    if (iVar1 + 1 > g_brCdTrackLast) {
      g_brCdTrackCur = g_brCdTrackFirst;
    }
    BrCdTrackPlay(g_brCdTrackCur);
  }
  return 1;
}

/* WHAT IT DOES: request CD track `param_1`: mark music pending, record the track, and if the
 * disc is readable and the player window is live, post it message 0x3B9 (play, 1). */
/* WHAT IT DOES: choose a CD music track at random, from track 3 up to the
 * last one on the disc. If the disc has more than six tracks it keeps drawing
 * until it gets one that is not the track already playing, so the music
 * actually changes; on a shorter disc it takes whatever it drew, repeat or
 * not. The clamps are the original's and do not depend on rand() behaving.
 * The one caller is BrRaceStep. */
/* @implements 0x10002C00 glide BrCdTrackRandom */

int BrCdTrackRandom(void)

{
  int iVar1;

  do {
    iVar1 = rand() * (g_brCdTrackLast - 5) / 0x8000 + 3;
    if (iVar1 < 3) {
      iVar1 = 3;
    }
    if (iVar1 > g_brCdTrackLast) {
      iVar1 = g_brCdTrackLast;
    }
  } while ((g_brCdTrackLast > 6) && (iVar1 == g_brCdTrackCur));
  return iVar1;
}

/* WHAT IT DOES: ask for a CD track to start playing. It records the wanted
 * track and raises the pending flag, then -- only if a disc is actually in the
 * drive and the window is up -- posts the message that makes the audio thread
 * act on it. With no disc the request is remembered but nothing is posted, so
 * it takes effect when the drive next reports media. Always reports success. */
/* @implements 0x10002BA0 glide BrCdTrackRequest */

int BrCdTrackRequest(int param_1)

{
  if ((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) {
    g_220CD8 = 1;
    g_brCdTrackCur = param_1;
    if ((g_brCdMediaOk != 0) && (DAT_1021c80c != '\0')) {
      PostMessageA(DAT_1021c77c,0x3b9,1,g_220C40);
    }
  }
  return 1;
}


/* WHAT IT DOES: apply a new CD-music enable byte: turning it on while a track is pending
 * resumes playback; turning it off (or already-on) with a pending track pauses MCI.
 * Always records the byte at 0x1021C80C and returns 1. */
/* @implements 0x10002DC0 glide BrCdEnableApply */

int BrCdEnableApply(char param_1)

{
  if ((param_1 == '\0') || (DAT_1021c80c != '\0')) {
    if ((param_1 == '\0') && ((DAT_1021c80c != '\0' && (g_220CD8 != 0)))) {
      BrCdMciPause();
    }
  }
  else if (g_220CD8 != 0) {
    DAT_1021c80c = param_1;
    BrCdTrackResume();
    DAT_1021c80c = param_1;
    return 1;
  }
  DAT_1021c80c = param_1;
  return 1;
}


#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif
extern int DAT_1021c77c;
extern int g_220C40;
extern int g_220CD8;
extern int g_brCdEnabled;
extern int g_brCdMediaOk;
extern int g_brCdPlaying;
extern int g_brCdTrackCur;
extern int g_brCdTrackFirst;
extern int g_brCdTrackLast;
extern char s_MCI_STATUS_returned__d_1007b07c[];
extern char s_cdaudio_1007b094[];
int BrSub10075020();

/* WHAT IT DOES: open the CD audio device and start it playing, on the FIRST
 * caller only -- later calls just raise the use count. Sets the device to
 * track-and-frame time format so later seeks can name a track. This is how
 * the in-game CD soundtrack starts. */
/* @implements 0x10002980 glide FUN_10002980 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_10002980(int param_1)

{
  unsigned int _Seed;
  MCIERROR MVar1;
  MCI_STATUS_PARMS status;
  MCI_SET_PARMS setp;
  MCI_OPEN_PARMS open;
  CHAR buf[1024];
  
  if ((g_brCdEnabled != 0) && (g_brCdPlaying = g_brCdPlaying + 1, g_brCdPlaying == 1)) {
    DAT_1021c77c = param_1;
    _Seed = BrSub10075020();
    srand(_Seed);
    g_brCdTrackCur = 2;
    g_brCdTrackFirst = 0;
    g_brCdTrackLast = 0;
    g_220CD8 = 0;
    g_brCdMediaOk = 0;
    open.lpstrDeviceType = s_cdaudio_1007b094;
    MVar1 = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE, (DWORD)&open);
    if (MVar1 != 0) {
      return 0;
    }
    g_220C40 = open.wDeviceID;
    setp.dwTimeFormat = MCI_FORMAT_TMSF;
    MVar1 = mciSendCommandA(open.wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)&setp);
    if (MVar1 != 0) {
      mciSendCommandA(g_220C40, MCI_CLOSE, 0, 0);
      return 0;
    }
    status.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
    MVar1 = mciSendCommandA(g_220C40, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD)&status);
    if (MVar1 != 0) {
      wsprintfA(buf, s_MCI_STATUS_returned__d_1007b07c, MVar1);
      OutputDebugStringA(buf);
      mciSendCommandA(g_220C40, MCI_CLOSE, 0, 0);
      return 0;
    }
    g_brCdTrackFirst = 2;
    g_brCdTrackLast = status.dwReturn;
    g_brCdMediaOk = 1;
  }
  return 1;
}


extern int g_220C40;
extern int g_brCdEnabled;
extern int g_brCdMediaOk;
extern int g_brCdPlaying;

/* WHAT IT DOES: ask the CD device what it is doing now and report its status
 * back. Answers zero without asking whenever CD audio is off, the drive is
 * empty, or nothing is playing. */
/* @implements 0x100027E0 glide FUN_100027e0 */
/* auto-filed from ghidra --refine; transforms: stackshred */

int FUN_100027e0(void)

{
  struct {
  char local_10 [4];
  int local_c;
  int local_8;
  int _pad_0;
  } _fr;

  
  if (((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) && (g_brCdMediaOk != 0)) {
    _fr.local_8 = 8;
    mciSendCommandA(g_220C40,0x814,0x100,(unsigned long)_fr.local_10);
    return _fr.local_c;
  }
  return 0;
}


extern int g_220C40;
extern int g_brCdTrackLast;

/* WHAT IT DOES: tell the CD device to play from a given track through to the
 * last one. Returns the raw MCI error code, so zero means it started. */
/* @implements 0x10002870 glide FUN_10002870 */
/* auto-filed from ghidra --refine; transforms: stackshred */

MCIERROR FUN_10002870(int param_1,unsigned char param_2)

{
  MCIERROR MVar1;
  struct {
  int local_c;
  unsigned int local_8;
  unsigned int local_4;
  } _fr;

  
  _fr.local_8 = param_2 & 0xff;
  _fr.local_4 = g_brCdTrackLast & 0xff;
  _fr.local_c = param_1;
  MVar1 = mciSendCommandA(g_220C40,0x806,0xd,(unsigned long)&_fr.local_c);
  if (MVar1 != 0) {
    mciSendCommandA(g_220C40,0x804,0,0);
    return MVar1;
  }
  return 0;
}


extern int DAT_1021c77c;
extern int g_brCdEnabled;
extern int g_brCdMediaOk;
extern int g_brCdPlaying;
extern int g_brCdTrackCur;

/* WHAT IT DOES: stop CD playback, reporting success. Like its siblings it
 * does nothing and claims success when CD audio is off or there is no disc,
 * so callers do not have to check first. */
/* @implements 0x10002830 glide FUN_10002830 */
/* MCI "advance track" poll: true unless the CD is enabled, playing and the
 * media is ready, in which case forward the current track to 0x10002870 and
 * report success as its result being zero.  param_2 is a BYTE there -- the
 * caller emits `mov al,[g_brCdTrackCur]`, so the callee takes unsigned char. */

int FUN_10002830(void)

{
  if (((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) && (g_brCdMediaOk != 0)) {
    return FUN_10002870(DAT_1021c77c,(unsigned char)g_brCdTrackCur) == 0;
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
