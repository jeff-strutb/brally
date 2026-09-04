/* br_cd.c -- audio.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#include <stdlib.h>

#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif

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

/* ---------------------------------------------------------------------------
 * 0x10002A20 (partial) -- 8-bit volume to the backend's 0..10000 scale.
 *
 * The original builds 10000*v with four `lea [r+r*4]` (x5 each, so x625) and
 * a `shl 4`, then divides by 255 with the signed magic 0x80808081 / sar 7 /
 * sign fixup. v is masked to 8 bits before any of that, so the input is
 * always non-negative and the sign fixup never fires.
 */
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

/* ==========================================================================
 * The CD track query
 * ========================================================================== */

/* br_data.c / slice2_11.h -- the CD module's globals. */
extern int g_brCdEnabled;     /* 0x100940A4, ships as 2 */
extern int g_brCdPlaying;     /* 0x10220CD0 */
extern int g_brCdTrackCur;    /* 0x10220CD4 */
extern int g_brCdMediaOk;     /* 0x10220C3C */

/* 0x10002490 */
/* WHAT IT DOES: reports which CD audio track is playing, or zero if there is
 * none -- CD music is off, nothing is playing, or the disc is not readable
 * all give zero. */
/* @implements 0x10002490 d3d BrCdTrackGetEar */
int BrCdTrackGetEar(void)
{
    /* Two success tests share one fail-out (`je` to `xor eax,eax / ret`).
     * Early `return 0` inverts the branches. */
    if (g_brCdEnabled != 0) {
        if (g_brCdPlaying != 0) {
            /* `neg eax / sbb eax, eax / and eax, ecx` -- a mask built from
             * g_brCdMediaOk and ANDed with the track, not a branch.  The
             * track is loaded either way. */
            return (g_brCdMediaOk != 0) ? g_brCdTrackCur : 0;
        }
    }
    return 0;
}

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

/* WHAT IT DOES: play the previous CD track, clamping to the first track. */
/* @implements 0x10002C70 glide BrCdTrackPrev */

int BrCdTrackPrev(void)

{
  int iVar1;

  if ((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) {
    iVar1 = FUN_10002c50();
    g_brCdTrackCur = iVar1 + -1;
    if (iVar1 + -1 < g_brCdTrackFirst) {
      g_brCdTrackCur = g_brCdTrackFirst;
    }
    BrCdTrackPlay(g_brCdTrackCur);
  }
  return 1;
}

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


extern int DAT_1021c788;

/* WHAT IT DOES: return the value of the global at 0x1021C788. */
/* @implements 0x100027A0 glide BrGetGlobal_1C788 */

int BrGetGlobal_1C788(void)

{
  return DAT_1021c788;
}

#endif /* BR_MATCHING_BUILD */
