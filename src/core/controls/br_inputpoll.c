/* br_inputpoll.c -- controls: the per-frame input poll.
 *
 * RESPONSIBILITY: reading what the player is doing -- one call per frame
 * that pulls the keyboard, joystick and mouse in through DirectInput, flips
 * the three double buffers, and folds every bound action into the flag word
 * the race step and the menus read.
 *
 * ONE FUNCTION: 0x100706D0 BrInputPoll (4,145 B; D3D twin 0x100773F0).
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
 * RESIDUE MAP / DEAD PROBES: see the notes above the function; updated as
 * the rounds progress.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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

/* WHAT IT DOES: the once-a-frame read of everything the player can touch.
 * It pulls the keyboard, the joystick (when one is configured) and the mouse
 * in through DirectInput, keeping this frame's and last frame's readings in
 * paired buffers so "just pressed" can be told from "held"; scales the
 * accumulated mouse movement by the sensitivity setting and clamps it; and
 * then walks the action bindings, turning each one that is active into a bit
 * of the flag word it returns and remembers.  Along the way it answers the
 * debug keys (F5-F10 toggles, F11/F12 CD tracks, F and P after frame 15),
 * lets Escape pause or quit, and, in benchmark mode, prints the frame rate
 * after 441 frames and exits.  The two out-parameters carry the analogue
 * steering and throttle amounts read from whichever axis is bound. */
/* @implements 0x100706D0 glide BrInputPoll */
uint32_t BrInputPoll(int32_t *pAxis0, int32_t *pAxis1)
{
    BrInMouseState ms;
    char           buf[256];
    uint32_t       flags;
    int32_t        hr;
    uint16_t       w;
    int32_t        g;
    int32_t        div;
    int32_t        cur;
    int32_t        prev;
    int32_t        now;
    int32_t        dt;
    int32_t        df;

    if (g_brInputFrame < 0x7FFF)
        g_brInputFrame++;
    flags = 0;

    /* ---- keyboard --------------------------------------------------- */
    g_brInKeyPrev = g_brInKeyCur;
    g_brInKeyCur = (g_brInKeyCur - 1) & 1;
    hr = g_pBrDik18ABDD0->pVtbl->GetDeviceState(g_pBrDik18ABDD0, 0x100u,
                                                g_brInKeys[g_brInKeyCur]);
    if (hr < 0) {
        if (hr == BR_DIERR_NOTACQUIRED) {
            hr = g_pBrDik18ABDD0->pVtbl->Acquire(g_pBrDik18ABDD0);
            if (hr < 0) {
                g_brInputLast = 0;
                return 0;
            }
            hr = g_pBrDik18ABDD0->pVtbl->GetDeviceState(g_pBrDik18ABDD0, 0x100u,
                                                        g_brInKeys[g_brInKeyCur]);
            if (hr < 0) {
                g_brInputLast = 0;
                return 0;
            }
        } else {
            g_brInputLast = 0;
            return 0;
        }
    }
    g_brInKeys[g_brInKeyCur][0] = 0;

    /* ---- joystick --------------------------------------------------- */
    if (g_brB4E1D0 == 1 || g_brB4E1D0 == 2) {
        g_brInJoyPrev = g_brInJoyCur;
        g_brInJoyCur = (g_brInJoyCur - 1) & 1;
        g_pBrInJoyDev->pVtbl->Poll(g_pBrInJoyDev);
        hr = g_pBrInJoyDev->pVtbl->GetDeviceState(g_pBrInJoyDev, 0x110u,
                                                  &g_brInJoy[g_brInJoyCur]);
        if (hr != 0) {
            if (hr == BR_DIERR_NOTACQUIRED)
                BrDiAcquire();
            memset(g_brInJoy[g_brInJoyCur].rgbButtons, 0, 0x80);
        }
    }

    /* ---- mouse ------------------------------------------------------ */
    g_brInMousePrev = g_brInMouseCur;
    g_brInMouseCur = (g_brInMouseCur - 1) & 1;
    if (g_pBrInDiRoot != 0 && g_pBrInDiRoot->pMouse != 0) {
        hr = g_pBrInDiRoot->pMouse->pVtbl->GetDeviceState(g_pBrInDiRoot->pMouse,
                                                          0x10u, &ms);
        if (hr == 0) {
            prev = g_brInMousePrev;
            cur = g_brInMouseCur;
            g_brInMouse[cur].ax = ms.lX + g_brInMouse[prev].ax;
            g_brInMouse[cur].ay = ms.lY + g_brInMouse[prev].ax;
            g_brInMouse[cur].az = ms.lZ + g_brInMouse[prev].ax;
            g = g_brMouseSens;
            if (g < 0)
                g = 0;
            else if (g > 7)
                g = 7;
            div = g_brMouseDivTable[g];
            g_brInMouse[cur].x = (g_brInMouse[cur].ax << 7) / div;
            g_brInMouse[cur].y = (g_brInMouse[cur].ay << 7) / div;
            g_brInMouse[cur].z = (g_brInMouse[cur].az << 7) / div;
            if (g_brInMouse[cur].x < -0x80) {
                g_brInMouse[cur].x = -0x80;
                g_brInMouse[cur].ax = -div;
            } else if (g_brInMouse[cur].x > 0x80) {
                g_brInMouse[cur].x = 0x80;
                g_brInMouse[cur].ax = div;
            }
            if (g_brInMouse[cur].y < -0x80) {
                g_brInMouse[cur].y = -0x80;
                g_brInMouse[cur].ay = -div;
            } else if (g_brInMouse[cur].y > 0x80) {
                g_brInMouse[cur].y = 0x80;
                g_brInMouse[cur].ay = div;
            }
            if (g_brInMouse[cur].z < -0x80) {
                g_brInMouse[cur].z = -0x80;
                g_brInMouse[cur].az = -div;
            } else if (g_brInMouse[cur].z > 0x80) {
                g_brInMouse[cur].z = 0x80;
                g_brInMouse[cur].az = div;
            }
            g_brInMouse[cur].buttons[0] = ms.rgbButtons[0];
            g_brInMouse[cur].buttons[1] = ms.rgbButtons[1];
            g_brInMouse[cur].buttons[2] = ms.rgbButtons[2];
            g_brInMouse[cur].buttons[3] = ms.rgbButtons[3];
        } else {
            if (hr == BR_DIERR_NOTACQUIRED)
                g_pBrInDiRoot->pMouse->pVtbl->Acquire(g_pBrInDiRoot->pMouse);
            memset(g_brInMouse[g_brInMouseCur].buttons, 0, 4);
        }
    } else {
        memset(&g_brInMouse[g_brInMouseCur], 0, sizeof(BrInMouse));
    }

    /* ---- Escape: pause, or leave -------------------------------------- */
    if (BrInputJustPressed(15)) {
        if (g_br10226A48 != 0 && g_br10226A44 != 0 && g_br105CCB88 == 0 &&
            g_br10AF21B0 < g_br100BCBE8) {
            BrSub10004F50();
        } else {
            g_br10226A50 = 1;
            g_brInputLast = 0x4000;
            return 0x4000;
        }
    }

    /* ---- debug keys: F5..F10 toggles, F11/F12 CD tracks ---------------- */
    if ((g_brInKeys[g_brInKeyPrev][0x3F] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x3F] & 0x80) != 0)
        g_br100BCBF0 = (g_br100BCBF0 == 0);
    if ((g_brInKeys[g_brInKeyPrev][0x40] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x40] & 0x80) != 0)
        g_br100BCBF4 = (g_br100BCBF4 == 0);
    if ((g_brInKeys[g_brInKeyPrev][0x41] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x41] & 0x80) != 0)
        g_br100BCBF8 = (g_br100BCBF8 == 0);
    if ((g_brInKeys[g_brInKeyPrev][0x42] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x42] & 0x80) != 0)
        g_br100BCBFC = (g_br100BCBFC == 0);
    if ((g_brInKeys[g_brInKeyPrev][0x43] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x43] & 0x80) != 0)
        g_br100BCC00 = (g_br100BCC00 == 0);
    if ((g_brInKeys[g_brInKeyPrev][0x44] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x44] & 0x80) != 0)
        g_br100BCC04 = (g_br100BCC04 == 0);
    if ((g_brInKeys[g_brInKeyPrev][0x57] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x57] & 0x80) != 0)
        BrCdTrackPrev();
    if ((g_brInKeys[g_brInKeyPrev][0x58] & 0x80) == 0 &&
        (g_brInKeys[g_brInKeyCur][0x58] & 0x80) != 0)
        BrCdTrackNext();

    if ((GetAsyncKeyState(0x46) & 1) && g_brInputFrame > 15)
        g_BrFpsGuard = (g_BrFpsGuard == 0);
    if ((GetAsyncKeyState(0x50) & 1) && g_brInputFrame > 15)
        g_br118EEEE0 = (g_br118EEEE0 == 0);

    /* ---- the non-race screens ------------------------------------------ */
    if (g_br105CCB88 != 0) {
        if (g_br105CCB88 == 2) {
            if (BrInputJustPressed(8))  flags |= 0x100;
            if (BrInputJustPressed(9))  flags |= 0x200;
            if (BrInputJustPressed(10)) flags |= 0x400;
        }
        if (BrInputJustPressed(11))   flags |= 0x800;
        if (BrInputJustPressed(0x15)) flags |= 0x100400;
        if (BrInputIsDown(0x16))      flags |= 0x200000;
        if (BrInputIsDown(0x17))      flags |= 0x400000;
        if (BrInputIsDown(0x18))      flags |= 0x800000;
        if (BrInputIsDown(0x19))      flags |= 0x1000000;
        if (BrInputJustPressed(0x1A)) flags |= 0x200000;
        if (BrInputJustPressed(0x1B)) flags |= 0x400000;
        g_brInputLast = flags;
        return flags;
    }

    /* ---- in the race ------------------------------------------------- */
    if (BrInputJustPressed(0x10)) {
        if (g_br105CCB5C == 0 && g_brCfgGameMode != 4 && g_brCfgGameMode != 5)
            g_br118EEEE4 = 1;
        if (g_br10226A48 != 0) {
            if (g_brRace18EEED8 == 0) {
                BrSub10004F20();
                g_brRace18EEED8 = 1;
            }
        } else {
            g_br10226A44 = 1;
            if (g_brCfgGameMode == 2)
                BrSub10063A40();
        }
    }
    if (BrInputJustPressed(14))
        flags |= 0x8000;

    *pAxis0 = 0;
    if (g_brCfgGameMode != 4 && g_brCfgGameMode != 5) {
        w = *(const uint16_t *)(const void *)g_BrPadModeBytes;
        if (w & 0x8000) {
            switch (w & 0xFF00) {
            case 0x8000:
                if (g_brInJoy[g_brInJoyCur].lX < 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lX);
                break;
            case 0x8100:
                if (g_brInJoy[g_brInJoyCur].lX > 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lX);
                break;
            case 0x8200:
                if (g_brInJoy[g_brInJoyCur].lY < 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lY);
                break;
            case 0x8300:
                if (g_brInJoy[g_brInJoyCur].lY > 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lY);
                break;
            case 0x8400:
                if (g_brInJoy[g_brInJoyCur].lZ < 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lZ);
                break;
            case 0x8500:
                if (g_brInJoy[g_brInJoyCur].lZ > 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lZ);
                break;
            case 0x8600:
                if (g_brInMouse[g_brInMouseCur].x < 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].x);
                break;
            case 0x8700:
                if (g_brInMouse[g_brInMouseCur].x > 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].x);
                break;
            case 0x8800:
                if (g_brInMouse[g_brInMouseCur].y < 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].y);
                break;
            case 0x8900:
                if (g_brInMouse[g_brInMouseCur].y > 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].y);
                break;
            case 0x8A00:
                if (g_brInMouse[g_brInMouseCur].z < 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].z);
                break;
            case 0x8B00:
                if (g_brInMouse[g_brInMouseCur].z > 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].z);
                break;
            }
        }
        w = *(const uint16_t *)(const void *)(g_BrPadModeBytes + 6);
        if (w & 0x8000) {
            switch (w & 0xFF00) {
            case 0x8000:
                if (g_brInJoy[g_brInJoyCur].lX < 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lX);
                break;
            case 0x8100:
                if (g_brInJoy[g_brInJoyCur].lX > 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lX);
                break;
            case 0x8200:
                if (g_brInJoy[g_brInJoyCur].lY < 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lY);
                break;
            case 0x8300:
                if (g_brInJoy[g_brInJoyCur].lY > 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lY);
                break;
            case 0x8400:
                if (g_brInJoy[g_brInJoyCur].lZ < 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lZ);
                break;
            case 0x8500:
                if (g_brInJoy[g_brInJoyCur].lZ > 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lZ);
                break;
            case 0x8600:
                if (g_brInMouse[g_brInMouseCur].x < 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].x);
                break;
            case 0x8700:
                if (g_brInMouse[g_brInMouseCur].x > 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].x);
                break;
            case 0x8800:
                if (g_brInMouse[g_brInMouseCur].y < 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].y);
                break;
            case 0x8900:
                if (g_brInMouse[g_brInMouseCur].y > 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].y);
                break;
            case 0x8A00:
                if (g_brInMouse[g_brInMouseCur].z < 0)
                    *pAxis0 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].z);
                break;
            case 0x8B00:
                if (g_brInMouse[g_brInMouseCur].z > 0)
                    *pAxis0 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].z);
                break;
            }
        }
    }

    /* ---- paused ------------------------------------------------------ */
    if (g_br105CCB5C != 0) {
        if (BrInputJustPressed(12)) flags |= 0x1000;
        if (BrInputJustPressed(13)) flags |= 0x2000;
        if (BrInputJustPressed(0))  flags |= 1;
        if (BrInputJustPressed(1))  flags |= 2;
        if (BrInputJustPressed(2))  flags |= 4;
        g_brInputLast = flags;
        return flags;
    }

    /* ---- throttle ---------------------------------------------------- */
    *pAxis1 = 0;
    if (BrInputIsDown(2)) {
        flags |= 4;
        if ((g_BrPadModeBytes[0xD] & 0x80) == 0)
            *pAxis1 = 0x50;
    }
    w = *(const uint16_t *)(const void *)(g_BrPadModeBytes + 0xC);
    if (w & 0x8000) {
        switch (w & 0xFF00) {
        case 0x8000:
            if (g_brInJoy[g_brInJoyCur].lX < 0)
                *pAxis1 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lX);
            break;
        case 0x8100:
            if (g_brInJoy[g_brInJoyCur].lX > 0)
                *pAxis1 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lX);
            break;
        case 0x8200:
            if (g_brInJoy[g_brInJoyCur].lY < 0)
                *pAxis1 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lY);
            break;
        case 0x8300:
            if (g_brInJoy[g_brInJoyCur].lY > 0)
                *pAxis1 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lY);
            break;
        case 0x8400:
            if (g_brInJoy[g_brInJoyCur].lZ < 0)
                *pAxis1 = BR_AXIS_SCALE_NEG(g_brInJoy[g_brInJoyCur].lZ);
            break;
        case 0x8500:
            if (g_brInJoy[g_brInJoyCur].lZ > 0)
                *pAxis1 = BR_AXIS_SCALE(g_brInJoy[g_brInJoyCur].lZ);
            break;
        case 0x8600:
            if (g_brInMouse[g_brInMouseCur].x < 0)
                *pAxis1 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].x);
            break;
        case 0x8700:
            if (g_brInMouse[g_brInMouseCur].x > 0)
                *pAxis1 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].x);
            break;
        case 0x8800:
            if (g_brInMouse[g_brInMouseCur].y < 0)
                *pAxis1 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].y);
            break;
        case 0x8900:
            if (g_brInMouse[g_brInMouseCur].y > 0)
                *pAxis1 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].y);
            break;
        case 0x8A00:
            if (g_brInMouse[g_brInMouseCur].z < 0)
                *pAxis1 = BR_AXIS_SCALE_NEG(g_brInMouse[g_brInMouseCur].z);
            break;
        case 0x8B00:
            if (g_brInMouse[g_brInMouseCur].z > 0)
                *pAxis1 = BR_AXIS_SCALE(g_brInMouse[g_brInMouseCur].z);
            break;
        }
    }

    if (BrInputIsDown(3))
        flags |= 8;
    if (BrInputIsDown(4)) {
        flags |= 0x10;
        *pAxis1 = -0x50;
    }
    if (g_brCfgGameMode != 4 && g_brCfgGameMode != 5) {
        if (BrInputIsDown(0)) flags |= 1;
        if (BrInputIsDown(1)) flags |= 2;
    }
    if (BrInputIsDown(5))         flags |= 0x20;
    if (BrInputIsDown(6))         flags |= 0x40;
    if (BrInputJustPressed(8))    flags |= 0x100;
    if (BrInputJustPressed(9))    flags |= 0x200;
    if (BrInputJustPressed(10))   flags |= 0x400;
    if (BrInputJustPressed(0x11)) flags |= 0x10000;
    if (BrInputJustPressed(0x12)) flags |= 0x20000;
    if (BrInputJustPressed(0x13)) flags |= 0x40000;
    if (BrInputJustPressed(0x14)) flags |= 0x80000;
    if (BrInputIsDown(7))         flags |= 0x80;

    /* ---- benchmark: time 440 frames, print the rate, leave ------------ */
    if (g_brCfgRunBenchmark != 0) {
        if (g_brInputFrame == 1) {
            g_br118EEE18 = BrSub10075020();
            g_br118EEE8C = BrGetFlag_AB4F0();
        }
        if (g_brInputFrame == 0x1B9) {
            now = BrSub10075020();
            dt = now - g_br118EEE18;
            g_br118EEE18 = now;
            df = BrGetFlag_AB4F0() - g_br118EEE8C;
            sprintf(buf, "fps = %0.2f", (float)df * 1000.0f / (float)dt);
            exit(1);
            BrLogPrint(buf);
        }
        GetAsyncKeyState(0x1B);
        if (g_brInputFrame == 1) {
            g_BrFpsGuard = 1;
            flags = 0x400;
        } else {
            flags = (g_brInputFrame < 60) ? 0 : 4;
        }
    }
    g_brInputLast = flags;
    return flags;
}
