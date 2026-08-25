/* br_netmsg.c -- the six DirectPlay menu-message senders, BRGlide.dll
 * 0x10036FF0..0x100371DD (D3D 0x1003D950..0x1003DB30).
 *
 * One shape, six tags: build an eight-byte packet {0x600000TT, payload} on
 * the stack and hand it to IDirectPlay4A::Send through slice1_03.h's
 * critical-section wrapper (BrComCallLocked68), guarding with ||-merged
 * early-outs that share a single `return 0`.  Tags 2/3/4/5 are gated on
 * 0x10AA288C being zero; tags 6/7 always send.  Tag 3 sends its second
 * payload dword UNINITIALISED (original defect, reproduced).  All six are
 * byte-identical to the original.  Filed out of slice4_50/52/53 and
 * slice6_70 per the file-as-you-match rule. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include "slice2_25.h"      /* BrOptUi, g_brAA288C */
#include "slice2_26.h"      /* BrObjA9D008 (tag 7's object view) */
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

/* 0x1003D950 */
/* WHAT IT DOES: sends one small message to the other players from a menu
 * screen -- a fixed tag and one number. What the number means was not
 * established. Whether it sends at all is gated on a global, so the same call
 * can silently do nothing. */
/* @implements 0x1003D950 d3d BrSub1003D950 */
int32_t BrSub1003D950(BrOptUi *pUi, int a)
{
    /* CONFLICT: slice2_25.h models BrOptUi as three int32_t, but +0x00 is
     * dereferenced as an object pointer and +0x08 is passed on as a pointer.
     * A pointer-sized view of the same object is used so a 64-bit host does
     * not truncate them; see the header. */
    void *const *aSlot = (void *const *)pUi;
    void        *pObj;
    void        *pArg;
    int32_t      aPacket[2];

    if (pUi == NULL || (pObj = aSlot[0]) == NULL || g_brAA288C != 0) {
        return 0;
    }
    pArg = aSlot[2];

    aPacket[0] = (int32_t)0x60000002u;
    aPacket[1] = (int32_t)a;

    /* (pObj, pArg, 0, 1, &packet, 8) -- IDirectPlay4A::Send through
     * slice1_03.h's critical-section wrapper. The original discards the
     * HRESULT. */
    return BrComCallLocked68((BrComObj *)pObj, pArg,
                             (void *)(uintptr_t)0u,
                             (void *)(uintptr_t)1u,
                             aPacket,
                             (void *)(uintptr_t)8u);
}

/* 0x1003D9A0 */
/* WHAT IT DOES: sends the tag-5 sibling of the message above -- same gate,
 * same shape, one payload number. */
/* @implements 0x1003D9A0 d3d BrSub1003D9A0 */
int32_t BrSub1003D9A0(BrOptUi *pUi, int a)
{
    /* Inlined send, tag 0x60000005; the shape (||-merged return-0
     * early-outs, HRESULT returned) is BrSub1003D950's above. */
    void *const *aSlot = (void *const *)pUi;
    void        *pObj;
    int32_t      aPacket[2];

    if (pUi == NULL || (pObj = aSlot[0]) == NULL || g_brAA288C != 0) {
        return 0;
    }
    aPacket[0] = (int32_t)0x60000005u;
    aPacket[1] = (int32_t)a;
    return BrComCallLocked68((BrComObj *)pObj, aSlot[2],
                             (void *)(uintptr_t)0u,
                             (void *)(uintptr_t)1u,
                             aPacket,
                             (void *)(uintptr_t)8u);
}

/* 0x1003DA90 */
/* WHAT IT DOES: sends the tag-6 sibling -- ungated, like tag 7. */
/* @implements 0x1003DA90 d3d BrSub1003DA90 */
int32_t BrSub1003DA90(BrOptUi *pUi, int a)
{
    void *const *aSlot = (void *const *)pUi;
    void        *pObj;
    int32_t      aPacket[2];

    if (pUi == NULL || (pObj = aSlot[0]) == NULL) {
        return 0;
    }
    aPacket[0] = (int32_t)0x60000006u;
    aPacket[1] = (int32_t)a;
    return BrComCallLocked68((BrComObj *)pObj, aSlot[2],
                             (void *)(uintptr_t)0u,
                             (void *)(uintptr_t)1u,
                             aPacket,
                             (void *)(uintptr_t)8u);
}

/* WHAT IT DOES: sends one particular kind of tagged message to the other
 * machines in a multiplayer game, carrying a value taken from a global. What
 * the message means to the receiver is not established here. */
/* @implements 0x1003D9F0 d3d BrSub1003D9F0 */
int32_t BrSub1003D9F0(struct BrOptUi *pUi)
{
    /* The original inlines the whole send (tag 0x60000003, no payload store:
     * the second packet dword is sent UNINITIALISED -- original defect,
     * reproduced by not writing it).  Early-outs share one `return 0`.
     * Shape identical to BrSub1003D950 (slice4_50.c), which see. */
    void *const *aSlot = (void *const *)pUi;
    void        *pObj;
    int32_t      aPacket[2];

    if (pUi == NULL || (pObj = aSlot[0]) == NULL || g_brAA288C != 0) {
        return 0;
    }
    aPacket[0] = (int32_t)0x60000003u;
    return BrComCallLocked68((BrComObj *)pObj, aSlot[2],
                             (void *)(uintptr_t)0u,
                             (void *)(uintptr_t)1u,
                             aPacket,
                             (void *)(uintptr_t)8u);
}

/* 0x1003DA40 */
/* WHAT IT DOES: sends one particular kind of small message to the other
 * players in a network game, if the network link is up. */
/* @implements 0x1003DA40 d3d BrSub1003DA40 */
int32_t BrSub1003DA40(BrOptUi *pUi, int a)
{
    /* Inlined send, tag 0x60000004 with `a` as the payload dword; the shape
     * (||-merged return-0 early-outs, HRESULT returned) is BrSub1003D950's,
     * which see in slice4_50.c. */
    void *const *aSlot = (void *const *)pUi;
    void        *pObj;
    int32_t      aPacket[2];

    if (pUi == NULL || (pObj = aSlot[0]) == NULL || g_brAA288C != 0) {
        return 0;
    }
    aPacket[0] = (int32_t)0x60000004u;
    aPacket[1] = (int32_t)a;
    return BrComCallLocked68((BrComObj *)pObj, aSlot[2],
                             (void *)(uintptr_t)0u,
                             (void *)(uintptr_t)1u,
                             aPacket,
                             (void *)(uintptr_t)8u);
}

/* 0x1003DB00 */
/* WHAT IT DOES: sends one of the ungated network messages to the other
 * players. Unlike the numbered messages that check a gate first, this one
 * always goes out; whatever it fails with is thrown away. */
/* @implements 0x1003DB00 d3d BrExt_1003DB00 */
int32_t BrExt_1003DB00(struct BrObjA9D008 *pObj, void *p)
{
    /* Inlined send, tag 0x60000007, UNGATED (no 0x10AA288C check -- what
     * makes tags 6/7/8 different from 2/3/4/5); the shape is otherwise
     * BrSub1003D950's (slice4_50.c), which see.  `p` occupies the second
     * payload dword, narrowed to 32 bits. */
    void *const *aSlot = (void *const *)pObj;
    void        *pIface;
    int32_t      aPacket[2];

    if (pObj == NULL || (pIface = aSlot[0]) == NULL) {
        return 0;
    }
    aPacket[0] = (int32_t)0x60000007u;
    aPacket[1] = (int32_t)(uintptr_t)p;
    return BrComCallLocked68((struct BrComObj *)pIface, aSlot[2],
                             (void *)(uintptr_t)0u,
                             (void *)(uintptr_t)1u,
                             aPacket,
                             (void *)(uintptr_t)8u);
}


#ifdef BR_MATCHING_BUILD
/* ------------------------------------------------------------------ */
/* 0x10037260 -- format-and-MessageBox                                 */
/* ------------------------------------------------------------------ */

__declspec(dllimport) int __cdecl wsprintfA(char *pDst, const char *pFmt, ...);
__declspec(dllimport) int __stdcall MessageBoxA(void *hWnd, const char *pText,
                                                const char *pCaption,
                                                unsigned int uType);
int FUN_100372b0();
const char *BrStrGet(int id);

/* WHAT IT DOES: resolve param_2 through 0x100372B0, format it with the given
 * pattern into a 200-byte stack buffer, and MessageBox it with string 0x126
 * as the caption. */
/* @implements 0x10037260 glide BrNetErrMsgBox */

void BrNetErrMsgBox(const char *param_1,int param_2)

{
  int uVar1;
  char local_c8 [200];

  uVar1 = FUN_100372b0(param_2);
  wsprintfA(local_c8,param_1,uVar1);
  MessageBoxA((void *)0x0,local_c8,BrStrGet(0x126),0);
  return;
}
#endif /* BR_MATCHING_BUILD */
