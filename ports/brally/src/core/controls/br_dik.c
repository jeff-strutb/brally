/* br_dik.c -- the per-frame keyboard poll and its edge detection.
 *
 * RESPONSIBILITY: reading what the player is doing -- pulling the DirectInput
 * keyboard state in, turning it into "pressed just now" answers, and the
 * device-slot housekeeping around it.
 *
 * Moved here out of the address batches under src/brally/core/; the bodies are the
 * text that was matched there, unchanged.
 */
#include "slice1_10.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include "slice3_39.h"
#include "slice1_07.h"   /* BrDevSlot -- see the note in slice3_39.h */
#include "slice6_72.h"   /* BrDInputDev / BR72_DIERR_NOTACQUIRED / Br72Env */

/* WHAT IT DOES: reports which key was newly pressed this frame, taking the
 * first one it finds, or -1 if none were. This is how a "press any key"
 * prompt is answered. */
/* @implements 0x1005FFD0 d3d BrFn1005FFD0 */
int32_t BrFn1005FFD0(void)
{
    int32_t i;

    for (i = 0; i < BR_DIK_COUNT; ++i) {
        if (g_BrDikEdge[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* WHAT IT DOES: read the keyboard and, if that succeeded, work out which
 * keys changed since last time. The per-frame input poll; a failed read
 * leaves the previous state alone rather than reporting everything as
 * released. */
/* @implements 0x10059020 glide BrDikPollAndEdge */
void BrDikPollAndEdge(void)
{
    if (BrDikGetDeviceState(g_BrDikState) >= 0) {
        BrMenuSub1005FF60();
    }
}

/* WHAT IT DOES: refreshes both sets of input edges for this frame --
 * keyboard keys and controller buttons -- so the menus can tell a fresh
 * press from a held one. */
/* @implements 0x1003E070 d3d BrFn1003E070 */
void BrFn1003E070(void)
{
    BrMenuSub1005FF60();
    BrInputLatchUpdate();
}

/* ==========================================================================
 * 0x100771B0
 * ========================================================================== */
/* WHAT IT DOES: reads which keys are down right now. If the game has lost the
 * keyboard to another program in the meantime it quietly grabs it back and
 * reads again. When there is no keyboard device at all it reports what looks
 * like success and leaves the caller's buffer holding whatever was in it
 * before, so keys can appear stuck. */
typedef int32_t (__stdcall *BrDiGetStateFn)(BrDInputDev *, uint32_t, void *);
typedef int32_t (__stdcall *BrDiAcquireFn)(BrDInputDev *);
/* Orig is `mov eax,[0x118eeee8]` three times, not an env-struct field. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: read the whole keyboard state array from DirectInput in one
 * go. Reports failure without touching the caller's buffer when there is no
 * device. */
/* @implements 0x10070490 glide BrDikGetDeviceState */
int32_t BrDikGetDeviceState(uint8_t *pState)
{
    BrDInputDev *pDev;
    int32_t      hr;
    uint8_t     *p;

    /* Load device first, then pState into esi so the NULL path still
     * `push esi` / `pop esi` (orig does not shrink-wrap the save). */
    pDev = g_pBrDik18ABDD0;
    p = pState;
    if (pDev == NULL) {
        hr = 1;
    } else {
        hr = ((BrDiGetStateFn)pDev->pVtbl->GetDeviceState)(pDev, 0x100u, p);
        if (hr < 0) {
            if (hr == BR72_DIERR_NOTACQUIRED) {
                pDev = g_pBrDik18ABDD0;
                hr = ((BrDiAcquireFn)pDev->pVtbl->Acquire)(pDev);
                if (hr >= 0) {
                    pDev = g_pBrDik18ABDD0;
                    hr = ((BrDiGetStateFn)pDev->pVtbl->GetDeviceState)(pDev, 0x100u, p);
                }
            }
        }
    }
    return hr;
}

/* WHAT IT DOES: translates a typed key into the character it should produce,
 * by walking a table until it finds a match and taking the first one. Keys
 * that are not in the table produce nothing -- and a handful of keys, Tab
 * among them, produce something only because the walk runs off the end of the
 * real table and keeps going through the data that happens to follow it. */
/* The matching build scans the image's own region: 784 eight-byte records
 * from 0x100AC5F8 to 0x100ADE78, the bound the original compares against.
 * The port's g_BrCharMap holds the 113 codes that region can actually match
 * (see slice3_39.h); ending the matching loop at ITS end named an address
 * 904 bytes in, and the function read as byte-exact only because the image
 * gate copied the original's bound. */
#define BR_CHARMAP_SCAN 784
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100AC5F8 */
#define g_BrCharMap g_BrCharMapImage
/* @implements 0x1005B540 d3d BrCharMapLookup */
uint8_t BrCharMapLookup(int32_t code)
{
    uint32_t i;

    for (i = 0; i < BR_CHARMAP_SCAN; ++i) {
        if (g_BrCharMapImage[268 + i].code == (uint32_t)code) {
            return (uint8_t)g_BrCharMapImage[268 + i].ch;
        }
    }
    return 0;
}
#undef g_BrCharMap

/* Storage in the same 0x118ABxxx input-globals block as the DirectInput
 * keyboard device pointer, reset by the routine below. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x118ABD38 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x118ABAD4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x118ABD80 */

/* WHAT IT DOES: zeroes 14 dwords at 0x118ABD38, writes 0 to 0x118ABAD4, and
 * writes 1 to 0x118ABD80. The three are separate globals, not one struct.
 *
 * GOTCHA: the two scalar stores are written first so MSVC 5 selects `C7 05`
 * immediates; it then schedules the `rep stosd` ahead of them. Writing the
 * zero-fill first CSE's the 0 into `mov [g_18ABAD4], eax`. */
/* @implements 0x100770C0 d3d BrSub100770C0 */
void BrSub100770C0(void)
{
    int i;

    (*(int *)&g_brInMousePrev) = 0;
    g_18ABD80 = 1;
    for (i = 0; i < 14; ++i)
        g_18ABD38[i] = 0;
}

/* WHAT IT DOES: if this slot still holds a device, and the current screen is
 * actually up, it asks that device to do one of two things. Which one is
 * picked by a global flag. The extra argument it is handed is never looked
 * at. */
/* Original is 2-arg thiscall: `this` in ecx, one unread stack dword, `ret 4`.
 * BR_THISCALL1 (= __fastcall) would put that dword in edx; a struct is never
 * register-eligible, so it is forced back onto the stack.
 * COM methods on the iface are stdcall (`push eax; call [vtbl+n]`): the
 * header's cdecl pointers would emit `add esp, 4` after each call. */
typedef struct { uint32_t v; } BrSub10060750Arg;
typedef struct {
    char pad[0x1C];
    void (__stdcall *pfn1C)(void *pThis);
    void (__stdcall *pfn20)(void *pThis);
} BrSub10060750Vtbl;
typedef struct {
    BrSub10060750Vtbl *pVtbl;
} BrSub10060750Iface;
typedef struct {
    uint32_t _00, _04, _08, f0C;
} BrSub10060750Phase;
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AA2904 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10AA33E0 */

/* WHAT IT DOES: tell a device slot's object to show or hide itself,
 * depending on whether the current screen is live and a related flag. */
/* @implements 0x10060750 d3d BrSub10060750 */
void BR_THISCALL1 BrSub10060750(BrDevSlot *pSlot)
{
    BrSub10060750Iface *pIface = (BrSub10060750Iface *)pSlot->pIface;
    uint32_t            live;
    uint32_t            flag;


    if (pIface != NULL) {
        live = (*(BrSub10060750Phase * *)&g_brPAA29B8)->f0C;
        if (live != 0) {
            flag = g_BrAA33E0;
            if (flag != 0) {
                pIface->pVtbl->pfn20(pIface);
            } else {
                pIface->pVtbl->pfn1C(pIface);
            }
        }
    }
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100724c0: prototype in br_funcs.h */
/* BrDiAcquire: prototype in br_funcs.h */
typedef int (__stdcall *CC_std_1)(int);
typedef int (__stdcall *CC_std_3)(int, int, int);

/* WHAT IT DOES: the joystick "press anything" scan for the control-binding
 * UI. Creates the joystick device on first use (marking the binding table
 * as the joystick page at 0x10B4E080 and calling the DirectInput init);
 * then polls it, reads a full DIJOYSTATE2, re-acquires on DIERR_NOTACQUIRED,
 * and reports the first pressed control: button i as *pOut=0x100 returning
 * i, or an axis pushed past +/-50 as *pOut=0x8000+0x100*dir returning 0.
 * Returns -1 when nothing is pressed, 1 when no device exists. */
/* @implements 0x100704E0 glide BrJoyScanAny */

int BrJoyScanAny(int *param_1)

{
  int iVar1;
  unsigned char auStack_110 [272];

  if ((*(int * *)&g_brFfb) == (int *)0x0) {
    (*(int *)&g_BrCtrlCfg.active) = 2;
    (*(int * *)&g_BrPadModeBytes) = &(*(int *)&g_BrCtrlCfg.profile[2].e[0][0]);
    BrFfbInit();
    if ((*(int * *)&g_brFfb) == (int *)0x0) {
      return 1;
    }
  }
  (*(CC_std_1 *)&((void **)*(void ***)(((*(int * *)&g_brFfb))))[25])(*(int * *)&g_brFfb);
  iVar1 = (*(CC_std_3 *)&((void **)*(void ***)(((*(int * *)&g_brFfb))))[9])((*(int * *)&g_brFfb),0x110,(int)auStack_110);
  if ((iVar1 != 0) && (iVar1 == -0x7ff8ffe2)) {
    BrDiAcquire();
  }
  /* A `for` here, not do-while: VC5 converts the known-true `for` into the
   * original's unrotated do-while with `test [esp+eax+0x30],cl` (the 0x80
   * shared between mask and bound in ecx); a source-spelled do-while gets
   * rotated with a peeled first-button load into dl (+4 B). */
  for (iVar1 = 0; iVar1 < 0x80; iVar1 = iVar1 + 1) {
    if ((auStack_110[iVar1 + 0x30] & 0x80) != 0) {
      *param_1 = 0x100;
      return iVar1;
    }
  }
  if (*(int *)auStack_110 < -0x32) {
    *param_1 = 0x8000;
    return 0;
  }
  if (0x32 < *(int *)auStack_110) {
    *param_1 = 0x8100;
    return 0;
  }
  if (*(int *)(auStack_110 + 4) < -0x32) {
    *param_1 = 0x8200;
    return 0;
  }
  if (0x32 < *(int *)(auStack_110 + 4)) {
    *param_1 = 0x8300;
    return 0;
  }
  return -1;
}
