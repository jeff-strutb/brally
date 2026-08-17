/* br_bootinit.h -- RESPONSIBILITY: bring the game up.  This is Glide
 * 0x10032530 (D3D 0x10038EC0), the FIRST call state 0 (0x1001CD70) makes,
 * before the menu sound bank is selected.
 *
 * WHAT IT IS: eleven calls, two guards and a tail jump.  121 bytes, no locals,
 * no arithmetic.  It is the point in the boot where the archive, the splash
 * screen, the damage bitmaps, the two DirectX device subsystems, the music and
 * the sound effects are all started, in that order.
 *
 * THE SEQUENCE, off the listing:
 *
 *   0x10032530  0x10008D20(this=0x10AC0810, "BossRally.pod")  __thiscall
 *   0x10032544  0x10008AB0(this=0x10AC0810)                   __thiscall
 *   0x1003254B  0x100639D0(3)
 *   0x1003255D  0x1006C990("splash.img", 0x2AC7E58B)
 *   0x10032565  0x1005A480()
 *   0x1003256A  0x10071FC0()
 *   0x1003256F  0x100703D0()
 *   0x10032574  if ([0x1007B074] != 0) {
 *   0x10032583      0x100028E0([0x105BC72C])
 *   0x1003258B      0x10059E00()
 *   0x10032592      0x10002AF0(2)
 *               }
 *   0x1003259A  if ([0x100B55F0] != 0) return 0x1006C4D0();   // tail jmp
 *   0x100325A8  return 0
 *
 * WHAT THE PIECES ARE, established rather than assumed:
 *
 *   0x10AC0810   the POD archive object.  0x10008D20 is `strcpy(this+0x20,
 *                name)` (MSVC's inlined `repne scasb` + `rep movsd`,
 *                __thiscall, `ret 4`); 0x10008AB0 opens that name, checks the
 *                three-byte magic against 0x1007B5BC and errors with "%s is
 *                not a valid POD file", then allocates 76 bytes per directory
 *                entry and reads the directory.  CONVENTIONS.md records that
 *                the shipped BossRally.pod holds ONE entry and is a leftover,
 *                so this succeeds and yields nothing.
 *   0x1006C990   the image loader/blitter -- the same function state 3 calls
 *                with "loading.img" (br_boot.c, 0x1001CDF6).  Two cdecl
 *                arguments; state 3 passes 0 where this passes 0x2AC7E58B.
 *   0x1005A480   loads "Paint\damage1.bmp" .. "damage3.bmp" into the three
 *                slots at 0x10AC67B0.
 *   0x10071FC0   COM init behind a `++guard != 1` gate, inside an SEH frame;
 *                on failure it puts up MessageBoxA(strings 0x127, 0x126) --
 *                "Unable to initialize DirectInput." / "Boss Rally Error".
 *   0x100703D0   the other device init, also behind a `++guard != 1` gate:
 *                clears 0x80 and 0x88 DWORDS of state, creates a device and
 *                sets its data format from 0x10078718.
 *   0x1007B074   the MUSIC BACKEND selector.  br_appstart.h has it as
 *                `PlayMusic=` from BossRally.cfg, shipped value 2; slice6_78.h
 *                and br_input.h both key behaviour off `== 1` (MCI) versus
 *                anything else (EAR).  The guard here is merely `!= 0`.
 *   0x100B55F0   `PlaySFX=` from the same file (slice1_08.c owns it as
 *                BrSndG0B5DE8, D3D 0x100B5DE8).
 *   0x100028E0   music-backend init; takes the HWND at 0x105BC72C, which the
 *                window procedure stored on WM_CREATE.  Dispatches on
 *                0x1007B074 == 1 exactly like its siblings.
 *   0x10059E00   ALREADY PORTED: D3D 0x10060D90 == slice6_76.c's
 *                BrSub10060D90, the two volume-slider tables.
 *   0x10002AF0   ALREADY PORTED: D3D 0x100027C0 == slice5_63.c's
 *                BrCdTrackPlay.  The argument is the literal 2.
 *   0x1006C4D0   DirectSound device creation, behind its own `++guard != 1`
 *                gate (slice1_08.h, br_sfx.h).  Reached by a TAIL JUMP, so
 *                its return value is 0x10032530's.
 *
 * TWO THINGS THE STRUCTURE SAYS THAT ARE EASY TO MISS
 *
 * The music block is THREE calls under ONE guard, and the volume tables and
 * the CD track are inside it -- so with PlayMusic=0 the volume sliders are
 * never initialised either, not just the music.
 *
 * The last guard is a tail JUMP and not a call, so this function's return
 * value is 0x1006C4D0's when sound is on and ZERO when it is off (eax still
 * holds the flag that just failed `test eax,eax`).  0x1001CD70 ignores it, but
 * modelling it as void would throw away the only value the function produces.
 *
 * WHY THE CALLEES ARE AN OPS STRUCT AND THE TWO FLAGS ARE ARGUMENTS
 *
 * Nine of the eleven callees are unported and two are owned by modules with
 * large closures, so calling any of them directly would make this six-line
 * function drag in half the tree -- the same trap br_bootfrontier.c's banner
 * describes.  They are supplied by the caller instead, and each entry says
 * which original address it stands for.  A NULL entry is SKIPPED and COUNTED
 * (BrBootColdInitSkipped) rather than faked: nothing here invents a result.
 *
 * The two flags are passed in for the same reason -- they are globals owned by
 * br_appstart / slice1_08 and reading them here would either couple the module
 * or, worse, define a second copy.  CONVENTIONS.md, "Aliased storage".
 */
#ifndef BR_BOOTINIT_H
#define BR_BOOTINIT_H

#include <stdint.h>

/* 0x100AA3D4 and 0x100AA3C8 -- the two string literals, exactly. */
#define BR_BOOTINIT_POD_NAME    "BossRally.pod"
#define BR_BOOTINIT_SPLASH_NAME "splash.img"

/* 0x10032553.  Pushed under "splash.img" as 0x1006C990's second cdecl
 * argument.  State 3 passes 0 in the same slot for "loading.img", so the
 * parameter is not a flag word that only ever takes one value. */
#define BR_BOOTINIT_SPLASH_ARG  0x2AC7E58Bu

/* 0x1003254B.  The literal 0x100639D0 is called with. */
#define BR_BOOTINIT_639D0_ARG   3

/* 0x10032590.  The literal BrCdTrackPlay is called with. */
#define BR_BOOTINIT_CD_TRACK    2

/* One per call site, in the order they occur.  Used only to report which ones
 * a run reached with no hook installed. */
typedef enum BrBootColdInitStep {
    BR_COLDINIT_POD_SETNAME = 0, /* 0x10008D20 */
    BR_COLDINIT_POD_OPEN,        /* 0x10008AB0 */
    BR_COLDINIT_100639D0,        /* 0x100639D0 */
    BR_COLDINIT_1006C990,        /* 0x1006C990 */
    BR_COLDINIT_1005A480,        /* 0x1005A480 */
    BR_COLDINIT_10071FC0,        /* 0x10071FC0 */
    BR_COLDINIT_100703D0,        /* 0x100703D0 */
    BR_COLDINIT_100028E0,        /* 0x100028E0 */
    BR_COLDINIT_10059E00,        /* 0x10059E00 == slice6_76.c BrSub10060D90 */
    BR_COLDINIT_10002AF0,        /* 0x10002AF0 == slice5_63.c BrCdTrackPlay  */
    BR_COLDINIT_1006C4D0,        /* 0x1006C4D0, the tail jump */
    BR_COLDINIT_COUNT
} BrBootColdInitStep;

typedef struct BrBootColdInitOps {
    /* The POD object is the global at 0x10AC0810 and both calls are
     * __thiscall on it.  It has no portable type here, so it is carried as an
     * opaque pointer the caller chooses; the original passes the address of
     * its own .data object. */
    void   *pPod;                                      /* 0x10AC0810 */
    void  (*pfnPodSetName)(void *pPod, const char *pszName);  /* 0x10008D20 */
    void  (*pfnPodOpen)(void *pPod);                          /* 0x10008AB0 */

    void  (*pfn100639D0)(int32_t a1);
    void  (*pfn1006C990)(const char *pszImage, uint32_t arg);
    void  (*pfn1005A480)(void);
    void  (*pfn10071FC0)(void);
    void  (*pfn100703D0)(void);

    /* The HWND at 0x105BC72C, and the call that takes it. */
    void   *hWnd;
    void  (*pfn100028E0)(void *hWnd);

    void  (*pfn10059E00)(void);          /* wire to BrSub10060D90 */
    void  (*pfn10002AF0)(int32_t track); /* wire to BrCdTrackPlay  */

    /* The tail jump.  Its return value is BrBootColdInit's. */
    int32_t (*pfn1006C4D0)(void);
} BrBootColdInitOps;

/* Glide 0x10032530 / D3D 0x10038EC0.
 *
 * `fPlayMusic` is [0x1007B074] and `fPlaySfx` is [0x100B55F0], both tested
 * only for non-zero.  Returns pfn1006C4D0's value when fPlaySfx is non-zero
 * and 0 otherwise -- see the tail-jump note in the banner. */
int32_t BrBootColdInit(const BrBootColdInitOps *pOps,
                       int32_t fPlayMusic, int32_t fPlaySfx);

/* How many times a step was reached with no hook installed.  Zero for a fully
 * wired run.  Not in the original. */
int32_t BrBootColdInitSkipped(BrBootColdInitStep step);
void    BrBootColdInitResetForTest(void);

#endif /* BR_BOOTINIT_H */
