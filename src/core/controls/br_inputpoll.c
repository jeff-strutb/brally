/* br_inputpoll.c -- controls: the per-frame input poll.
 *
 * RESPONSIBILITY: reading what the player is doing -- one call per frame
 * that pulls the keyboard, joystick and mouse in through DirectInput, flips
 * the three double buffers, and folds every bound action into the flag word
 * the race step and the menus read.
 *
 * FUNCTIONS: 0x10071710 BrInputIsDown (695 B, byte-exact) and 0x100719D0
 * BrInputJustPressed (1,246 B, byte-exact; D3D twin 0x100786E0, whose port
 * body stays in slice3_45.c).  0x100706D0 BrInputPoll (4,145 B; D3D twin
 * 0x100773F0) is byte-exact in the C++ lane,
 * src/core/controls/BrInputPoll_100706D0.cpp: the original TU was C++, and
 * the notes below are the C transcription's history, kept for its dead list.
 * Neighbours 0x10070370 BrOnActivate and 0x10070170 BrWaveSeekData live in
 * br_input.c; 0x10070490 BrDikGetDeviceState in br_dik.c.
 *
 * Transcribed from the BRGlide.dll bytes, not from the Ghidra draft: the
 * draft mis-modelled the frame (unaff_EBX/EBP/ESI locals) and turned the two
 * out-parameters into stack slots.  The frame is 0x110 bytes: a 16-byte
 * DIMOUSESTATE at esp+0x10 and a 256-byte sprintf buffer at esp+0x20; the
 * two parameters are read late at esp+0x124 / esp+0x128.
 *
 * Shape notes, all read off the bytes:
 *   - ebx is the pinned zero, ebp the flag accumulator (`xor ebx,ebx; xor
 *     ebp,ebp` straight after the pushes).  `mov ebp,0x100` / `mov ebp,
 *     0x8000` are `flags |= K` with ebp provably still zero.
 *   - every exit stores the result to 0x118EEBE8 before returning it.
 *   - the three axis switches are `cmp; jg; je; cmp; jne` binary trees:
 *     real `switch` statements on the binding word masked to its high byte
 *     (the compacted node form -- BrInputIsDown's original has the other one).
 *   - `x * 80 / 128` is `lea [x+x*4]; shl 4; cdq; and 0x7f; add; sar 7`; the
 *     negated direction is `neg; shl 2; sub; shl 4` i.e. `x * -80`.
 *   - the mouse accumulate reads the PREVIOUS record's ax for all three axes
 *     (+0x5C three times) -- a copy-paste in the original, transcribed as is.
 *   - `memset(buttons, 0, 4)` is the `lea; mov [reg],ebx` dword zero; the
 *     0x80-byte joystick button wipe and the 0x1C mouse record wipe are the
 *     `rep stosd` intrinsic.
 *
 * C-LANE STATE (2026-09-05): 4145/4145 B, 1185/1185 instructions, register-blind
 * 0+0, ONE region of 2 bytes at orig+0x34.  Everything else in the function
 * is byte-identical.  Two source facts closed the rest of the residue:
 *
 *   1. THE FRAME (0x118 -> 0x110).  The benchmark block converts an unsigned
 *      divisor through an 8-byte temp (`mov [esp+0x14],ebx` zeroes the high
 *      half, `fild`/`fidiv` read the low dword).  The original parks that
 *      temp ON TOP OF the dead DIMOUSESTATE (esp+0x10); ours gave it its own
 *      8 bytes while `ms` was a function-scope local.  Declaring `ms` INSIDE
 *      the mouse block lets the slot be reused.  (The 2026-09-03 note that
 *      /O2 slot packing "ignores scope" was measured on scalars; an
 *      ADDRESS-TAKEN aggregate is different -- its block scope ends its
 *      lifetime for the packer.)
 *   2. THE MOUSE ACCUMULATE.  The original reads the previous record's `ax`
 *      once by scaled index (`mov esi,[ecx*4+A]`), keeps the ADDRESS
 *      (`lea eax,[ecx*4+A]`) and reads the two copy-pasted terms through it
 *      (`add edx,[eax]`).  Written three times as `g_brInMouse[prev].ax`
 *      VC5 forms the address once and reads through it all three times, and
 *      hoists the three DIMOUSESTATE loads into esi/edi ahead of the index
 *      arithmetic (+1 instruction, 3+2 register-blind).  A pointer to the
 *      FIELD, `int32_t *pPrevAx = &g_brInMouse[g_brInMousePrev].ax`, used
 *      for all three reads, is byte-exact: the C front end folds the first
 *      `*pPrevAx` back into the direct load and keeps the pointer for the
 *      rest.  A pointer to the RECORD (`pPrev->ax`) is size-exact but reads
 *      `[eax+0xc]` (+1 insn); operand order, `int ms[4]`, and `ms` through
 *      a pointer are all dead (see below).
 *
 * THE 2-BYTE RESIDUE, orig+0x34: keyboard arm, `mov edx,[ecx]` (vtable) then
 * `mov [g_brInKeyCur],eax` in the original; we emit the store first.  VC5's
 * C front end will not move a pointer deref above a store to a global (the
 * whole binary has exactly THREE deref-then-global-store adjacencies and the
 * other two, 0x1003BCA0 and 0x100382D0, are plain source order).  A source
 * order that puts the read first needs a local, and every local floats to
 * the top of the block instead.  DEAD, all measured (a8 = 2 B unless said):
 *   - the store inside the argument, `g_brInKeys[g_brInKeyCur = ...]` (=)
 *   - an `idx` local, stored then indexed (=)
 *   - `g_brInKeyCur` file-static (=); `volatile` + idx local (micro: =)
 *   - vtable field `const`, device pointer `const *`, `* const` (=, =, =)
 *   - `#pragma optimize("a"|"w", on)`: -220 B, the function falls apart
 *   - a device local assigned BEFORE the prev store: dev load hoists to +0x25
 *     (5 diffs); assigned AFTER the prev store: identical to a8
 *   - a VTABLE local, in any position (top, after idx, with/without a device
 *     local, via a comma expression, via a `*volatile*` deref): the dev load
 *     hoists to +0x21 and the vtable read to +0x30 (13-14 diffs)
 *   - the prev store as an absolute deref to anchor the dev load: +1 insn
 *   - the store target as a struct member / array element (micro: =)
 *   - the whole TU compiled as C++ (COM struct with virtual __stdcall slots,
 *     /O2 /GX /MD): THIS SITE IS BYTE-EXACT -- C1XX treats the vptr load
 *     as non-aliasing -- but the mouse site above then regresses to the
 *     plain-form shape under every C++ spelling tried (plain, field pointer,
 *     record pointer, const pointer, reference, value+pointer, register,
 *     function-scope, arithmetic/cast/flat-int pointer, ms via pointer).
 *   ONE front end built the original.  Under C the residue is these 2
 *   bytes; under C++ it is the mouse site (+1 insn, -3 B).  If the C++ lane
 *   finds a C1XX spelling for the mouse accumulate, this file moves there.
 *   2026-10-03: found -- an inline helper given the record INDEX and the
 *   previous ax and the DIMOUSESTATE by pointer (BrInMouseAccum in the
 *   C++ file).  BrInputPoll moved to the C++ lane byte-exact.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Local declarations.  Everything this TU needs is declared here so that
 * no shared header has to move; the names are the ones the rest of the
 * tree already uses for these addresses.
 * ------------------------------------------------------------------ */

/* IDirectInputDevice2A: slot 7 (+0x1C) Acquire, slot 9 (+0x24)
 * GetDeviceState(cbData, lpvData), slot 25 (+0x64) Poll. */
typedef struct BrInDiDev     BrInDiDev;
typedef struct BrInDiDevVtbl BrInDiDevVtbl;
struct BrInDiDevVtbl {
    void   *aReserved00[7];                                     /* +0x00 */
    int32_t (__stdcall *Acquire)(BrInDiDev *pThis);             /* +0x1C */
    void   *f20;                                                /* +0x20 */
    int32_t (__stdcall *GetDeviceState)(BrInDiDev *pThis,
                                        uint32_t cb, void *pv); /* +0x24 */
    void   *aReserved28[15];                                    /* +0x28 */
    int32_t (__stdcall *Poll)(BrInDiDev *pThis);                /* +0x64 */
};
struct BrInDiDev { const BrInDiDevVtbl *pVtbl; };

#define BR_DIERR_NOTACQUIRED  ((int32_t)0x8007001E)

/* The DirectInput root record (0x10AC61E0 points at it); the mouse device
 * sits at +0x50. */
typedef struct BrInDiRoot {
    uint8_t    pad00[0x50];
    BrInDiDev *pMouse;                                          /* +0x50 */
} BrInDiRoot;

/* DIJOYSTATE2 (0x110 bytes) and DIMOUSESTATE (0x10 bytes). */
typedef struct BrInJoy {
    int32_t  lX, lY, lZ;               /* +0x00 +0x04 +0x08 */
    int32_t  lRx, lRy, lRz;            /* +0x0C +0x10 +0x14 */
    int32_t  rglSlider[2];             /* +0x18              */
    uint32_t rgdwPOV[4];               /* +0x20              */
    uint8_t  rgbButtons[128];          /* +0x30              */
    uint8_t  pad[0x110 - 0xB0];        /* +0xB0 velocities.. */
} BrInJoy;
typedef struct BrInMouseState {
    int32_t lX, lY, lZ;                /* +0x00 +0x04 +0x08 */
    uint8_t rgbButtons[4];             /* +0x0C              */
} BrInMouseState;

/* The game's own mouse record: scaled axes, raw accumulators, buttons. */
typedef struct BrInMouse {
    int32_t x, y, z;                   /* +0x00 +0x04 +0x08  scaled     */
    int32_t ax, ay, az;                /* +0x0C +0x10 +0x14  accumulated */
    uint8_t buttons[4];                /* +0x18                          */
} BrInMouse;

extern int32_t     g_brInputFrame;        /* 0x118EEEF4 frames polled, caps at 0x7FFF */
extern int32_t     g_brInputLast;         /* 0x118EEBE8 the flags last returned        */
extern int32_t     g_brInKeyPrev;         /* 0x118EE9CC                                 */
extern int32_t     g_brInKeyCur;          /* 0x118EEBF0                                 */
extern int32_t     g_brInJoyPrev;         /* 0x118EEE94                                 */
extern int32_t     g_brInJoyCur;          /* 0x118EEBD0                                 */
extern int32_t     g_brInMousePrev;       /* 0x118EEBEC                                 */
extern int32_t     g_brInMouseCur;        /* 0x118EEE98                                 */
extern uint8_t     g_brInKeys[2][256];    /* 0x118EE9D0                                 */
extern BrInJoy     g_brInJoy[];           /* 0x118EEBF8, stride 0x110                   */
extern BrInMouse   g_brInMouse[];         /* 0x118EEE50, stride 0x1C                    */
extern BrInDiDev  *g_pBrDik18ABDD0;       /* 0x118EEEE8 keyboard device                 */
extern BrInDiDev  *g_pBrInJoyDev;         /* 0x118EEEEC joystick device                 */
extern BrInDiRoot *g_pBrInDiRoot;         /* 0x10AC61E0                                 */
extern int32_t     g_brB4E1D0;            /* 0x10B71530 controller kind (1/2 = stick)   */
extern const unsigned char *g_BrPadModeBytes; /* 0x10B71534 the bindings, 6 B each     */
extern int32_t     g_brMouseSens;         /* 0x10AF20A0 sensitivity index 0..7          */
extern const int32_t g_brMouseDivTable[8];/* 0x100BCC08 {803,618,475,366,281,216,166,128} */
extern int32_t     g_br10226A48;          /* 0x10226A48                                 */
extern int32_t     g_br10226A44;          /* 0x10226A44                                 */
extern int32_t     g_br10226A50;          /* 0x10226A50                                 */
extern int32_t     g_br105CCB88;          /* 0x105CCB88                                 */
extern int32_t     g_br105CCB5C;          /* 0x105CCB5C race paused                     */
extern int32_t     g_br10AF21B0;          /* 0x10AF21B0                                 */
extern int32_t     g_br100BCBE8;          /* 0x100BCBE8 lap count                       */
extern int32_t     g_br100BCBF0;          /* 0x100BCBF0 F5 debug toggle                 */
extern int32_t     g_br100BCBF4;          /* 0x100BCBF4 F6                              */
extern int32_t     g_br100BCBF8;          /* 0x100BCBF8 F7                              */
extern int32_t     g_br100BCBFC;          /* 0x100BCBFC F8                              */
extern int32_t     g_br100BCC00;          /* 0x100BCC00 F9                              */
extern int32_t     g_br100BCC04;          /* 0x100BCC04 F10                             */
extern int32_t     g_BrFpsGuard;          /* 0x10B73538 'F' toggle                      */
extern int32_t     g_br118EEEE0;          /* 0x118EEEE0 'P' toggle                      */
extern int32_t     g_brCfgGameMode;       /* 0x100A9360                                 */
extern int32_t     g_br118EEEE4;          /* 0x118EEEE4                                 */
extern int32_t     g_brRace18EEED8;       /* 0x118EEED8                                 */
extern int32_t     g_brCfgRunBenchmark;   /* 0x118EEEDC                                 */
extern int32_t     g_br118EEE18;          /* 0x118EEE18 benchmark start time            */
extern int32_t     g_br118EEE8C;          /* 0x118EEE8C benchmark start frame           */

uint8_t BrInputJustPressed(int32_t action);   /* 0x100719D0 */
uint8_t BrInputIsDown(int32_t action);        /* 0x10071710 */
int     BrDiAcquire(void);                    /* 0x100706B0 */
void    BrSub10004F50(void);                  /* 0x10004F50 */
int     BrCdTrackPrev(void);                  /* 0x10002C70 */
int     BrCdTrackNext(void);                  /* 0x10002CB0 */
void    BrSub10063A40(void);                  /* 0x10063A40 */
void    BrSub10004F20(void);                  /* 0x10004F20 */
int32_t BrSub10075020(void);                  /* 0x1006E280 millisecond clock */
int     BrGetFlag_AB4F0(void);                /* 0x10013FC0 frame counter     */
void    BrLogPrint(const char *psz);          /* 0x10008EF0 fatal screen      */

__declspec(dllimport) short __stdcall GetAsyncKeyState(int vk);

/* `x * 80 / 128`: an axis in +-128 scaled to +-80. */
#define BR_AXIS_SCALE(v)   ((v) * 80 / 128)
#define BR_AXIS_SCALE_NEG(v) ((v) * -80 / 128)

/* WHAT IT DOES: answers "is the player holding down the control for this
 * action right now?" -- checking whichever key, button, stick direction or
 * mouse movement the action is bound to, plus up to two keyboard alternatives
 * that always apply. Stick and mouse directions only count once they are
 * pushed past a dead zone, so a resting stick reads as nothing.
 *
 * The original indexes the key/button tables with UNCHECKED bytes (no & 1 /
 * & 3 masks). A plain switch on the u16 binding word masked to its high
 * byte (VC5 lowers it to the cmp/jg/je tree); the alternates test the whole
 * u16 word & 0xFF00, which VC5 folds to `test byte [b+3], 0xff`. */
/* @implements 0x10071710 glide BrInputIsDown */
uint8_t BrInputIsDown(int32_t action)
{
    uint8_t r = 0;
    const uint8_t *b = g_BrPadModeBytes + 6 * action;
    switch (*(const uint16_t *)(const void *)b & 0xFF00) {
    case 0x0000:
        r = (uint8_t)(g_brInKeys[g_brInKeyCur][b[0]] & 0x80u);
        break;
    case 0x0100:
        r = (uint8_t)(g_brInJoy[g_brInJoyCur].rgbButtons[b[0]] & 0x80u);
        break;
    case 0x0300:
        r = (uint8_t)(g_brInMouse[g_brInMouseCur].buttons[b[0]] & 0x80u);
        break;
    case 0x8000:
        if (g_brInJoy[g_brInJoyCur].lX < -50) r = 0x80;
        break;
    case 0x8100:
        if (g_brInJoy[g_brInJoyCur].lX > 50) r = 0x80;
        break;
    case 0x8200:
        if (g_brInJoy[g_brInJoyCur].lY < -50) r = 0x80;
        break;
    case 0x8300:
        if (g_brInJoy[g_brInJoyCur].lY > 50) r = 0x80;
        break;
    case 0x8400:
        if (g_brInJoy[g_brInJoyCur].lZ < -50) r = 0x80;
        break;
    case 0x8500:
        if (g_brInJoy[g_brInJoyCur].lZ > 50) r = 0x80;
        break;
    case 0x8600:
        if (g_brInMouse[g_brInMouseCur].x < -50) r = 0x80;
        break;
    case 0x8700:
        if (g_brInMouse[g_brInMouseCur].x > 50) r = 0x80;
        break;
    case 0x8800:
        if (g_brInMouse[g_brInMouseCur].y < -50) r = 0x80;
        break;
    case 0x8900:
        if (g_brInMouse[g_brInMouseCur].y > 50) r = 0x80;
        break;
    case 0x8A00:
        if (g_brInMouse[g_brInMouseCur].z < -50) r = 0x80;
        break;
    case 0x8B00:
        if (g_brInMouse[g_brInMouseCur].z > 50) r = 0x80;
        break;
    }
    if ((*(const uint16_t *)(const void *)(b + 2) & 0xFF00) == 0)
        r |= (uint8_t)(g_brInKeys[g_brInKeyCur][b[2]] & 0x80u);
    if ((*(const uint16_t *)(const void *)(b + 4) & 0xFF00) == 0)
        r |= (uint8_t)(g_brInKeys[g_brInKeyCur][b[4]] & 0x80u);
    return r;
}

/* The rising edge of one control: up last frame, down this frame.  Written
 * as an __inline function with an explicit `return 1; return 0;` because
 * that is what the bytes say: the three button arms of 0x100719D0
 * materialise the answer as a full-width `mov eax,1` / `xor eax,eax` (the
 * inliner's int return temp) and then keep only its low byte, while the
 * axis arms and the two keyboard tails work in the byte register.  The
 * same test written in place -- `&&` into the byte, `?:`, if/else, or an
 * inline body with a single `return` expression -- all compile to
 * `mov al,1` and lose the shared `xor eax,eax` exit. */
static __inline int BrInEdge(uint8_t prev, uint8_t cur)
{
    if ((prev & 0x80) == 0 && (cur & 0x80) != 0)
        return 1;
    return 0;
}

/* WHAT IT DOES: answers "did the player press this control on THIS frame?"
 * by comparing this frame's reading of the bound key, button or axis with
 * last frame's -- it is what stops a held key repeating in the menus.  The
 * binding's primary control may be a key, a joystick or mouse button (1 on
 * the press) or a joystick/mouse axis crossing the +-50 dead zone (0x80 on
 * the crossing); its two alternates are keyboard keys only and are ORed in
 * as 1.  The mouse button index is NOT masked (the D3D twin's `& 3` is a
 * port deviation). */
/* @implements 0x100719D0 glide BrInputJustPressed */
uint8_t BrInputJustPressed(int32_t action)
{
    /* r before b: the `xor al,al` lands ahead of the binding address and
     * pushes the argument into ecx (b first puts it in eax). */
    uint8_t r = 0;
    const unsigned char *b = g_BrPadModeBytes + action * 6;

    switch (*(const uint16_t *)(const void *)b & 0xFF00) {
    case 0x0000:
        r = BrInEdge(g_brInKeys[g_brInKeyPrev][b[0]], g_brInKeys[g_brInKeyCur][b[0]]);
        break;
    case 0x0100:
        r = BrInEdge(g_brInJoy[g_brInJoyPrev].rgbButtons[b[0]], g_brInJoy[g_brInJoyCur].rgbButtons[b[0]]);
        break;
    case 0x0300:
        r = BrInEdge(g_brInMouse[g_brInMousePrev].buttons[b[0]], g_brInMouse[g_brInMouseCur].buttons[b[0]]);
        break;
    case 0x8000:
        if (g_brInJoy[g_brInJoyPrev].lX >= -50 && g_brInJoy[g_brInJoyCur].lX < -50)
            r = 0x80;
        break;
    case 0x8100:
        if (g_brInJoy[g_brInJoyPrev].lX <= 50 && g_brInJoy[g_brInJoyCur].lX > 50)
            r = 0x80;
        break;
    case 0x8200:
        if (g_brInJoy[g_brInJoyPrev].lY >= -50 && g_brInJoy[g_brInJoyCur].lY < -50)
            r = 0x80;
        break;
    case 0x8300:
        if (g_brInJoy[g_brInJoyPrev].lY <= 50 && g_brInJoy[g_brInJoyCur].lY > 50)
            r = 0x80;
        break;
    case 0x8400:
        if (g_brInJoy[g_brInJoyPrev].lZ >= -50 && g_brInJoy[g_brInJoyCur].lZ < -50)
            r = 0x80;
        break;
    case 0x8500:
        if (g_brInJoy[g_brInJoyPrev].lZ <= 50 && g_brInJoy[g_brInJoyCur].lZ > 50)
            r = 0x80;
        break;
    case 0x8600:
        if (g_brInMouse[g_brInMousePrev].x >= -50 && g_brInMouse[g_brInMouseCur].x < -50)
            r = 0x80;
        break;
    case 0x8700:
        if (g_brInMouse[g_brInMousePrev].x <= 50 && g_brInMouse[g_brInMouseCur].x > 50)
            r = 0x80;
        break;
    case 0x8800:
        if (g_brInMouse[g_brInMousePrev].y >= -50 && g_brInMouse[g_brInMouseCur].y < -50)
            r = 0x80;
        break;
    case 0x8900:
        if (g_brInMouse[g_brInMousePrev].y <= 50 && g_brInMouse[g_brInMouseCur].y > 50)
            r = 0x80;
        break;
    case 0x8A00:
        if (g_brInMouse[g_brInMousePrev].z >= -50 && g_brInMouse[g_brInMouseCur].z < -50)
            r = 0x80;
        break;
    case 0x8B00:
        if (g_brInMouse[g_brInMousePrev].z <= 50 && g_brInMouse[g_brInMouseCur].z > 50)
            r = 0x80;
        break;
    }

    if ((*(const uint16_t *)(const void *)(b + 2) & 0xFF00) == 0)
        r |= (g_brInKeys[g_brInKeyPrev][b[2]] & 0x80) == 0
          && (g_brInKeys[g_brInKeyCur][b[2]] & 0x80) != 0;
    if ((*(const uint16_t *)(const void *)(b + 4) & 0xFF00) == 0)
        r |= (g_brInKeys[g_brInKeyPrev][b[4]] & 0x80) == 0
          && (g_brInKeys[g_brInKeyCur][b[4]] & 0x80) != 0;
    return r;
}

extern int *DAT_118eeee8;
extern int DAT_118eeef0;
typedef int (__stdcall *CC_std_1)();   /* COM method: this + arguments */

/* WHAT IT DOES: drop one user of the keyboard system and release the
 * DirectInput device when the last one goes. Clamps its own counter at zero,
 * so an unmatched release is ignored rather than driving the count negative. */
/* @implements 0x10071EB0 glide BrDiKeyboardShutdown */
void BrDiKeyboardShutdown(void)

{
  DAT_118eeef0 = DAT_118eeef0 + -1;
  if (DAT_118eeef0 < 0) {
    DAT_118eeef0 = 0;
    return;
  }
  if ((DAT_118eeef0 == 0) && (DAT_118eeee8 != (int *)0x0)) {
    (*(CC_std_1 *)(*(int *)(DAT_118eeee8) + 32))(DAT_118eeee8);
    (*(CC_std_1 *)(*(int *)(DAT_118eeee8) + 8))(DAT_118eeee8);
    DAT_118eeee8 = (int *)0x0;
  }
  return;
}
