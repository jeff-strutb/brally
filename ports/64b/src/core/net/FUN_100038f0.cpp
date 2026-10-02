/* WHAT IT DOES: consumes one received network packet and applies every
 * command in it to the local peer table: a peer's join/state header (name,
 * flags, id, with the whole slot reset when a new peer takes it, and the
 * local player's own bookkeeping when the header is its own), the sixteen
 * nibble pairs of the lobby table, a full or delta car-state sample slotted
 * into the peer's eight-entry ring (with the interpolation base picked from
 * the two newest full samples), the nine race-control messages of command
 * 0x60 (lobby return, kick, phase changes, leave, finish -- each formatting
 * a chat line from the peer's name), a timing pair, and the host's session
 * header; an unknown command is reported on stderr and ends the packet. */
/* @t3 0x100038F0 2026-09-16 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 4240/3799 insns 1341/1047 rows 30+324 regions 22 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register allocation in the command loop (the original re-reads
 * `t`/nMode/the WaitForSingleObject import from memory in every case; ours
 * hoists `t` into ebp and the import into esi at the loop head) plus the switch
 * jump tables MSVC lays inside our .text after the ret (cpp_score/divergence
 * count them; the original's sit past its 3799 extent). The A5 oracle runs the
 * dispatcher on 48 valid-state seeds and returns EQUIVALENT -- return + every
 * in-image global write + side effects agree -- so nothing is missing or wrong.
 * Byte-exactness is walled on that allocation, not on logic; see the @t4-pass
 * ledger below. Do not reopen before the end-grind. */
/* @implements 0x100038F0 glide FUN_100038f0
 * @cpp_kind free
 * @cpp_symbol _FUN_100038f0
 *
 * 3799 B cdecl EH-frame packet dispatcher. The reader is a 0x214-byte
 * packet object (bit-stream header + 0x200 payload) constructed on the
 * frame with a THISCALL ctor (two stack args, `ret 8`) and torn down by
 * the unwind action (the out-of-line nop 0x10008D60); every read is a
 * thiscall member, which is what routes this function to the C++ lane
 * (same evidence as the car-state codec pair).
 *
 * T2 2026-09-13 (C++ lane, first transcription): 3799 B orig, 4240 B ours
 * of which ~312 B are the switch tables that follow the ret in .text (the
 * cpp score counts them; divergence.py decodes them as code). Real residue
 * ~130 B in ~20 regions. What is already proven:
 *   - every slot field is its OWN global symbol plus `slot * 0x978`
 *     (`SLOTF`/`SLOTA`): a struct array or a (char *)base + off CSEs the
 *     full address into a register, the original CSEs only the product;
 *   - the two decode arms are `>= 2` first (big arm falls through, the
 *     scratch decode sits at the end);
 *   - the timeout formula reads DAT_10226a2c ONCE into a local (`mul esi`),
 *     `x % 100 / 0x21 + x / 100 * 3`;
 *   - the reset loop is a pointer walk with a counted `do { } while (--n)`,
 *     not `for (i < 8)` (VC5 turns that into three rep stosd);
 *   - `nName = i` is stored BEFORE the NUL at szName[0x18].
 *   - the 0x60 arms are laid out in SOURCE order: 0/1, 5, 4, 6, 7, 8 (case 5,
 *     the mutex + inner switch, precedes case 4 in the original).
 * Open walls, all allocation:
 *   - the original never hoists anything out of the packet loop (`t`,
 *     nMode, the import addresses are re-read from memory in every case);
 *     ours hoists `t` into ebp and the WaitForSingleObject import into
 *     esi at the loop head. Dead: t as a union, as a byte array behind an
 *     int cast, volatile t, volatile nMode (all measured). Compiling WITHOUT
 *     /Oi (strcpy/strcat as calls) makes 0x2f..0x337 byte-exact INCLUDING
 *     the frame slots, but the original's strcpy/strcat ARE inline, and
 *     `#pragma intrinsic` restores the /O2 shape -- the accident says the
 *     wall is register pressure in case 0, not the loop shape.
 *   - frame 0x790 vs 0x794: one 4-byte temp short next to the fild qword
 *     temp of case 0x80 (the original zeroes [esp+0x44] as well as the
 *     high dword at [esp+0x4c]).
 *   - slot order of the thirteen scalar locals is NOT name-keyed at /O2
 *     (a full rename gives identical bytes) and not declaration-keyed
 *     (declaring them in the original's frame order changes nothing).
 *
 * @t4-pass 0x100038F0 1 2026-09-13 probes 13 bytes 4240 insns 1341 regions 22 rows 354 census no  (first-transcription byte grind: t as a union / byte-array-behind-int-cast / volatile t / volatile nMode; /Oi off (0x2f..0x337 goes byte-exact but the original's strcpy/strcat ARE inline) then #pragma intrinsic restore; full local rename; declaration in the original's frame order; frame 0x790-vs-0x794 temp. All allocation; numbers unmoved.)
 * @t4-pass 0x100038F0 2 2026-09-16 probes 20 bytes 4240 insns 1341 regions 22 rows 354 census yes  (five more source levers -- volatile nMode (2654, worse), #pragma intrinsic, register loop vars, decl reorder, tb scalar -- none beats 2647; the A5 oracle runs the packet dispatcher on 48 valid-state seeds and returns EQUIVALENT: return + every in-image global write + side effects agree, so the residue is case-0 register allocation, not missing/wrong code. Numbers unmoved.)
 */
#define _CRTIMP __declspec(dllimport)
#include "slice3_41.h"   /* br_globals: its objects */
#include "slice1_02.h"
#include <string.h>
#include <stdio.h>

struct BrNetHdr {
    int f00;
    int f04;
    int f08;
};

/* VA-encoded class name (ctor_dtor) + m_<VA> methods, so the T3 oracle's
 * reloc resolver maps every thiscall to its real address.  Byte-neutral: the
 * calls are relocations, only the symbol strings differ. */
class BrNetPacket_1006CDA0_10008D60 {
public:
    BrNetPacket_1006CDA0_10008D60(void *pBuf, int nBytes);  /* ctor 0x1006CDA0 */
    ~BrNetPacket_1006CDA0_10008D60();                       /* dtor 0x10008D60 (nop) */
    void           m_1006CDD0();            /* Reset */
    unsigned char  m_1006CE00();            /* ReadU8 */
    unsigned short m_1006CE20();            /* ReadU16 */
    int            m_1006CE50();            /* ReadU24 */
    int            m_1006CE80();            /* ReadS32 */
    int            m_1006CF80();            /* AtEnd */
    BrNetHdr      *m_1006D190();            /* GetHdr */

    int            readBit;                 /* +0x00 */
    int            readByte;                /* +0x04 */
    int            writeBit;                /* +0x08 */
    int            writeByte;               /* +0x0C */
    unsigned char *pBuf;                    /* +0x10 */
    unsigned char  payload[0x200];          /* +0x14 */
};
typedef BrNetPacket_1006CDA0_10008D60 BrNetPacket;



/* BrCarState, BrNetSlot: slice1_02.h */



/* the "peer" walk below is the race-car table seen from each car's
 * DirectPlay player field (+0x144): g_aBrRaceCar */

extern "C" {
/* 64-bit core: WaitForSingleObject is declared by the platform headers */
/* 64-bit core: ReleaseMutex is declared by the platform headers */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrTicks30FromMs: prototype in br_funcs.h */
/* FUN_10004ad0: prototype in br_funcs.h */
/* BrCarStateDecode: prototype in br_funcs.h */
/* BrCarStateDecodeDelta: prototype in br_funcs.h */
/* BrCarStateLerp: prototype in br_funcs.h */
/* BrNetSlotGetF004: prototype in br_funcs.h */
/* BrNetSlotName: prototype in br_funcs.h */
/* FUN_100038a0: prototype in br_funcs.h */
/* BrComHolderRelease: prototype in br_funcs.h */
/* BrNetSlotGetF02C: prototype in br_funcs.h */
/* BrNetSlotSetF02C: prototype in br_funcs.h */
/* BrSub10075020: prototype in br_funcs.h */
/* Fn04C80: prototype in br_funcs.h */
/* BrNetPingSync: prototype in br_funcs.h */
/* FUN_10004d30: prototype in br_funcs.h */
/* FUN_10004900: prototype in br_funcs.h */
}


extern "C" void FUN_100038f0(void *pNet, void *pBuf, int nBytes, int nMode)
{
    BrNetPacket pkt(pBuf, nBytes);
    int        bGo;
    unsigned char tb[4];
    unsigned   b0;
    unsigned   b1;
    int        id;
    int        kind;
    char       c2;
    int        n;
    char       c3;
    float      frac;
    char       c4;
    int        nName;
    unsigned char nib;
    int        dt;
    unsigned   cmd;
    unsigned   slot;
    int        i;
    int       *pRing;
    BrCarState *pCar;
    unsigned   x;
    BrNetHdr  *pHdr;
    BrDriverCar *pPeer;
    char      *psz;
    char       szName[0x400];
    BrCarState scratch2;
    BrCarState scratch;

    WaitForSingleObject(DAT_1021ce4c, 0xffffffff);
    bGo = DAT_1021c900;
    ReleaseMutex(DAT_1021ce4c);
    if (bGo) {
        pkt.m_1006CDD0();
        (*(int *)tb) = pkt.m_1006CE50();
        WaitForSingleObject(g_hBrNetMutex, 0xffffffff);
        g_brNetPktTick = BrTicks30FromMs();
        ReleaseMutex(g_hBrNetMutex);
        while (!pkt.m_1006CF80()) {
            cmd = pkt.m_1006CE00();
            slot = cmd & 0xf;
            switch (cmd & 0xe0) {
            case 0x00:
                if (nMode != 1)
                    goto done;
                b0 = pkt.m_1006CE00();
                b1 = pkt.m_1006CE00();
                c2 = pkt.m_1006CE00();
                c3 = pkt.m_1006CE00();
                c4 = pkt.m_1006CE00();
                id = pkt.m_1006CE80();
                nName = 0;
                kind = b0 & 0x3f;
                if (kind <= 2) {
                    for (i = 0; i < 0x18; i++)
                        szName[i] = pkt.m_1006CE00();
                    nName = i;
                    szName[0x18] = 0;
                }
                if (kind == 4)
                    DAT_10226a2c = pkt.m_1006CE50();
                WaitForSingleObject(((g_aBrNetSlot[slot].hMutex)), 0xffffffff);
                if (slot != (unsigned)g_id) {
                    if (((g_aBrNetSlot[slot].f02C)) != (int)b0 && kind == 2) {
                        WaitForSingleObject(DAT_10226a5c, 0xffffffff);
                        DAT_1021c904 += 1;
                        (*(unsigned int (*)[])&DAT_1021c8c0)[DAT_1021c904] = slot;
                        ReleaseMutex(DAT_10226a5c);
                        ((g_aBrNetSlot[slot].f008)) = 0;
                        memset((&g_aBrNetSlot[slot].f00C[0]), 0, 8 * sizeof(unsigned));
                        ((g_aBrNetSlot[slot].f02C)) = 0;
                        pRing = (&g_aBrNetSlot[slot].f038[0]);
                        pCar = (&g_aBrNetSlot[slot].cars[0]);
                        n = 8;
                        do {
                            *pRing = 0;
                            memset(pCar, 0, sizeof(BrCarState));
                            pRing++;
                            pCar++;
                            n--;
                        } while (n != 0);
                        ((g_aBrNetSlot[slot].f558)) = 0;
                        ((g_aBrNetSlot[slot].f55C)) = 0;
                        ((g_aBrNetSlot[slot].f560)) = -1;
                        ((g_aBrNetSlot[slot].f568)) = 0;
                        ((g_aBrNetSlot[slot].f56C)) = 0;
                        ((g_aBrNetSlot[slot].f564)) = 0;
                        ((g_aBrNetSlot[slot].f974)) = 0;
                    }
                }
                if (slot == (unsigned)g_id) {
                    if (kind == 3) {
                        WaitForSingleObject(g_brH221324, 0xffffffff);
                        DAT_102265d8 = 0;
                        ReleaseMutex(g_brH221324);
                    }
                    if (slot == (unsigned)g_id) {
                        if (b0 & 0x80) {
                            WaitForSingleObject(g_brH22AF04, 0xffffffff);
                            (*(int *)&g_br22AAF4) = 0;
                            ReleaseMutex(g_brH22AF04);
                            DAT_10226a50 = 1;
                        }
                        if (b0 & 0x40) {
                            if (DAT_10226a50 != 0)
                                DAT_10226a50 = 0;
                            if ((*(int *)&g_BrX06909B4) != 0)
                                (*(int *)&DAT_105ccb68[6]) = 1;
                            WaitForSingleObject(g_brH220DDC, 0xffffffff);
                            DAT_1021ce44 = 0;
                            ReleaseMutex(g_brH220DDC);
                        }
                        if (slot == (unsigned)g_id && kind == 4) {
                            unsigned now;

                            WaitForSingleObject(DAT_1021c81c, 0xffffffff);
                            now = BrTicks30FromMs();
                            x = DAT_10226a2c;
                            DAT_10226a30 = x % 100 / 0x21 + x / 100 * 3;
                            if (DAT_10226a30 > now + 0x5a)
                                DAT_10226a30 = now + 0x5a;
                            ReleaseMutex(DAT_1021c81c);
                        }
                    }
                }
                ((g_aBrNetSlot[slot].f008)) = (*(int *)tb);
                ((g_aBrNetSlot[slot].f02C)) = b0;
                ((g_aBrNetSlot[slot].f030)) = b1;
                (&g_aBrNetSlot[slot].f034[0])[0] = c2;
                (&g_aBrNetSlot[slot].f034[0])[1] = c3;
                (&g_aBrNetSlot[slot].f034[0])[2] = c4;
                ((g_aBrNetSlot[slot].f004)) = id;
                if (nName)
                    strcpy((&g_aBrNetSlot[slot].szName[0]), szName);
                BrNetSend4AD0(pNet, slot, b1, c2, c3, c4, id, szName, b0, 0x10);
                ReleaseMutex(((g_aBrNetSlot[slot].hMutex)));
                break;

            case 0x20: {
                int *p;

                if (nMode != 1)
                    goto done;
                WaitForSingleObject(DAT_10226a58, 0xffffffff);
                for (p = DAT_1021ce00; (uintptr_t)p < (uintptr_t)&DAT_1021ce00[16]; p += 2) {
                    nib = pkt.m_1006CE00();
                    p[1] = nib >> 4;
                    p[0] = nib & 0xf;
                }
                ReleaseMutex(DAT_10226a58);
                break;
            }

            case 0x40: {
                int      best;
                unsigned bestT;
                int      d;

                WaitForSingleObject(((g_aBrNetSlot[slot].hMutex)), 0xffffffff);
                if ((((g_aBrNetSlot[slot].f02C)) & 0x3f) >= 2) {
                    best = 0;
                    bestT = 0xffffffff;
                    for (i = 0; i < 8; i++) {
                        if ((&g_aBrNetSlot[slot].f00C[0])[i] <= bestT) {
                            best = i;
                            bestT = (&g_aBrNetSlot[slot].f00C[0])[i];
                        }
                    }
                    ((g_aBrNetSlot[slot].f55C)) = best;
                    ((g_aBrNetSlot[slot].f558)) += 1;
                    (&g_aBrNetSlot[slot].f00C[0])[best] = (*(int *)tb);
                    (&g_aBrNetSlot[slot].f038[0])[((g_aBrNetSlot[slot].f55C))] = 0x40;
                    BrCarStateDecode(&(&g_aBrNetSlot[slot].cars[0])[((g_aBrNetSlot[slot].f55C))], (struct BrBitReader *)&pkt);
                    d = (g_brNetPktTick - (*(int *)tb)) * 2;
                    ((g_aBrNetSlot[slot].f974)) = (d % 3) * 0x21 + (d / 3) * 100;
                    ReleaseMutex(((g_aBrNetSlot[slot].hMutex)));
                } else {
                    BrCarStateDecode(&scratch, (struct BrBitReader *)&pkt);
                    ReleaseMutex(((g_aBrNetSlot[slot].hMutex)));
                }
                break;
            }

            case 0x60:
                switch (0x60000000 | (((*(int *)tb) & 0xff) << 16) | ((*(int *)tb) & 0xff00) | tb[2]) {
                case 0x60000000:
                case 0x60000001:
                    szName[0] = 0;
                    if ((*(int *)&g_BrCarCount) > 0) {
                        pPeer = g_aBrRaceCar;
                        for (i = 0; i < (*(int *)&g_BrCarCount); i++, pPeer++) {
                            if (nMode == BrNetSlotGetF004(pPeer->iNetPlayer)) {
                                psz = BrNetSlotName(pPeer->iNetPlayer);
                                strcpy(szName, psz);
                                strcat(szName, DAT_1007b304);
                                break;
                            }
                        }
                    }
                    pHdr = pkt.m_1006D190();
                    strcat(szName, (char *)&pHdr->f04);
                    FUN_100038a0(szName);
                    goto done;

                case 0x60000005:
                    pHdr = pkt.m_1006D190();
                    WaitForSingleObject(DAT_10226a54, 0xffffffff);
                    switch (pHdr->f04) {
                    case 4:
                        DAT_10226a28 = 0x14;
                        ReleaseMutex(DAT_10226a54);
                        break;
                    case 5:
                        DAT_10226a28 = 0x15;
                        ReleaseMutex(DAT_10226a54);
                        break;
                    case 6:
                        DAT_10226a28 = 0x16;
                        ReleaseMutex(DAT_10226a54);
                        break;
                    case 7:
                        DAT_10226a28 = 0x17;
                    default:
                        ReleaseMutex(DAT_10226a54);
                        break;
                    }
                    goto done;

                case 0x60000004:
                    pHdr = pkt.m_1006D190();
                    if (pHdr->f04 == BrNetSlotGetF004(g_id)) {
                        BrComHolderRelease();
                        DAT_10ac5bec = 1;
                        if ((*(int *)&g_BrCarCount) > 0) {
                            pPeer = g_aBrRaceCar;
                            for (i = 0; i < (*(int *)&g_BrCarCount); i++, pPeer++) {
                                if (nMode == BrNetSlotGetF004(pPeer->iNetPlayer)) {
                                    psz = BrNetSlotName(pPeer->iNetPlayer);
                                    strcpy(szName, psz);
                                    strcat(szName, s_booted_you_from_the_game__1007b2e8);
                                    FUN_100038a0(szName);
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                case 0x60000006:
                    pHdr = pkt.m_1006D190();
                    if (pHdr->f04 == nMode) {
                        if ((*(int *)&g_BrCarCount) > 0) {
                            pPeer = g_aBrRaceCar;
                            for (i = 0; i < (*(int *)&g_BrCarCount); i++, pPeer++) {
                                if (nMode == BrNetSlotGetF004(pPeer->iNetPlayer)) {
                                    if (BrNetSlotGetF02C(pPeer->iNetPlayer) & 0x3f) {
                                        WaitForSingleObject(g_h1022AF30, 0xffffffff);
                                        (*(int *)&g_i10221318) += 1;
                                        g_a10221288[(*(int *)&g_i10221318)] = pPeer->iNetPlayer;
                                        ReleaseMutex(g_h1022AF30);
                                        BrNetSlotSetF02C(i, 0);
                                        psz = BrNetSlotName(pPeer->iNetPlayer);
                                        strcpy(szName, psz);
                                        strcat(szName, s_left_the_race__1007b2d8);
                                        FUN_100038a0(szName);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                case 0x60000007:
                    pHdr = pkt.m_1006D190();
                    if (pHdr->f04 == nMode) {
                        if ((*(int *)&g_BrCarCount) > 0) {
                            pPeer = g_aBrRaceCar;
                            for (i = 0; i < (*(int *)&g_BrCarCount); i++, pPeer++) {
                                if (nMode == BrNetSlotGetF004(pPeer->iNetPlayer)) {
                                    psz = BrNetSlotName(pPeer->iNetPlayer);
                                    strcpy(szName, psz);
                                    strcat(szName, s_returned_to_race_lobby__1007b2bc);
                                    FUN_100038a0(szName);
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                case 0x60000008:
                    pHdr = pkt.m_1006D190();
                    if (nMode == 1) {
                        if ((*(int *)&g_BrCarCount) > 0) {
                            pPeer = g_aBrRaceCar;
                            for (i = 0; i < (*(int *)&g_BrCarCount); i++, pPeer++) {
                                if (pHdr->f04 == BrNetSlotGetF004(pPeer->iNetPlayer)) {
                                    int k = pHdr->f08;

                                    if (k >= 0 && k < 8) {
                                        psz = BrNetSlotName(pPeer->iNetPlayer);
                                        sprintf(szName, s__s_finished__s_1007b2ac, psz, PTR_s_First__100aa3e8[k]);
                                        FUN_100038a0(szName);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                default:
                    goto done;
                }

            case 0x80: {
                int      best, iNew, iPrev;
                unsigned bestT, tNew, tPrev;
                int      d;

                WaitForSingleObject(((g_aBrNetSlot[slot].hMutex)), 0xffffffff);
                if ((((g_aBrNetSlot[slot].f02C)) & 0x3f) >= 2) {
                    best = 0;
                    bestT = 0xffffffff;
                    for (i = 0; i < 8; i++) {
                        if ((&g_aBrNetSlot[slot].f00C[0])[i] <= bestT) {
                            bestT = (&g_aBrNetSlot[slot].f00C[0])[i];
                            best = i;
                        }
                    }
                    iNew = 0;
                    tNew = 0;
                    for (i = 0; i < 8; i++) {
                        if ((&g_aBrNetSlot[slot].f038[0])[i] == 0x40 && tNew < (&g_aBrNetSlot[slot].f00C[0])[i]) {
                            iNew = i;
                            tNew = (&g_aBrNetSlot[slot].f00C[0])[i];
                        }
                    }
                    iPrev = 0;
                    tPrev = 0;
                    for (i = 0; i < 8; i++) {
                        if ((&g_aBrNetSlot[slot].f038[0])[i] == 0x40 && tPrev < (&g_aBrNetSlot[slot].f00C[0])[i] && i != iNew) {
                            iPrev = i;
                            tPrev = (&g_aBrNetSlot[slot].f00C[0])[i];
                        }
                    }
                    dt = (&g_aBrNetSlot[slot].f00C[0])[iNew] - (&g_aBrNetSlot[slot].f00C[0])[iPrev];
                    if (dt == 0)
                        frac = 1.0f;
                    else
                        frac = (float)(unsigned)((*(int *)tb) - (&g_aBrNetSlot[slot].f00C[0])[iPrev]) / dt;
                    ((g_aBrNetSlot[slot].f55C)) = best;
                    ((g_aBrNetSlot[slot].f558)) += 1;
                    (&g_aBrNetSlot[slot].f00C[0])[best] = (*(int *)tb);
                    (&g_aBrNetSlot[slot].f038[0])[((g_aBrNetSlot[slot].f55C))] = 0x80;
                    BrCarStateLerp(&(&g_aBrNetSlot[slot].cars[0])[((g_aBrNetSlot[slot].f55C))], frac,
                                   &(&g_aBrNetSlot[slot].cars[0])[iPrev], &(&g_aBrNetSlot[slot].cars[0])[iNew]);
                    BrCarStateDecodeDelta(&(&g_aBrNetSlot[slot].cars[0])[((g_aBrNetSlot[slot].f55C))], &(&g_aBrNetSlot[slot].cars[0])[iNew], (struct BrBitReader *)&pkt);
                    d = (g_brNetPktTick - (*(int *)tb)) * 2;
                    ((g_aBrNetSlot[slot].f974)) = (d % 3) * 0x21 + (d / 3) * 100;
                    ReleaseMutex(((g_aBrNetSlot[slot].hMutex)));
                } else {
                    BrCarStateDecodeDelta(&scratch2, &scratch2, (struct BrBitReader *)&pkt);
                    ReleaseMutex(((g_aBrNetSlot[slot].hMutex)));
                }
                break;
            }

            case 0xc0: {
                int      a, b;
                unsigned short c;

                if (nMode != 1)
                    goto done;
                a = BrSub10075020();
                b = pkt.m_1006CE50();
                c = pkt.m_1006CE20();
                WaitForSingleObject(((g_aBrNetSlot[slot].hMutex)), 0xffffffff);
                ((g_aBrNetSlot[slot].f970)) = b;
                ((g_aBrNetSlot[slot].f974)) = c;
                ReleaseMutex(((g_aBrNetSlot[slot].hMutex)));
                if (slot == (unsigned)g_id) {
                    Fn04C80(pNet, (*(unsigned int *)tb));
                    BrNetPingSync((*(int *)tb), a, b, c);
                }
                break;
            }

            case 0xe0:
                if (nMode != 1)
                    goto done;
                g_id = slot;
                BrPalFetch();
                (*(int *)&g_brCfgPlayers) = pkt.m_1006CE00();
                (*(int *)&g_Br0B380C) = pkt.m_1006CE00();
                g_226e80 = pkt.m_1006CE00();
                DAT_1021ce50 = pkt.m_1006CE20();
                DAT_1021cdb0 = pkt.m_1006CE00();
                DAT_10226a40 = pkt.m_1006CE00();
                DAT_10226a3c = pkt.m_1006CE00();
                BrNetSend4900(pNet, slot, (*(char *)&g_aBrRaceCar[0].f29AC), (*(char *)&g_aBrRaceCar[0].f29AD), (*(char *)&g_aBrRaceCar[0].f29AE), &(g_aBrCfgPlayerName[0]), 0x10);
                BrNetSlotSetF02C(slot, 2);
                break;

            default:
                fprintf(stderr, s_Error__unknown_command__02X_rece_1007b26c, cmd & 0xe0, slot);
                goto done;
            }
        }
    }
done:
    ;
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x1006CDA0: the original calls BrBitStreamInit by address */
BrNetPacket_1006CDA0_10008D60::BrNetPacket_1006CDA0_10008D60(void * a1, int a2)
{
    BrBitStreamInit((struct BrBitStream *)this, (void *)a1, (int)a2);
}

/* 0x10008D60: the original calls BrPodNop by address */
BrNetPacket_1006CDA0_10008D60::~BrNetPacket_1006CDA0_10008D60()
{
    BrPodNop();
}

/* 0x1006CDD0: the original calls BrPairReset_10073B90 by address */
void BrNetPacket_1006CDA0_10008D60::m_1006CDD0()
{
    BrPairReset_10073B90((unsigned int *)this);
}

/* 0x1006CE00: the original calls BrBitStreamReadU8 by address */
unsigned char BrNetPacket_1006CDA0_10008D60::m_1006CE00()
{
    return (unsigned char)BrBitStreamReadU8((struct BrBitStream *)this);
}

/* 0x1006CE20: the original calls BrBitStreamReadU16 by address */
unsigned short BrNetPacket_1006CDA0_10008D60::m_1006CE20()
{
    return (unsigned short)BrBitStreamReadU16((struct BrBitStream *)this);
}

/* 0x1006CE50: the original calls BrBitStreamReadU24 by address */
int BrNetPacket_1006CDA0_10008D60::m_1006CE50()
{
    return (int)BrBitStreamReadU24((struct BrBitStream *)this);
}

/* 0x1006CE80: the original calls BrBitStreamReadS32 by address */
int BrNetPacket_1006CDA0_10008D60::m_1006CE80()
{
    return (int)BrBitStreamReadS32((struct BrBitStream *)this);
}

/* 0x1006CF80: the original calls BrBitStreamAtEnd by address */
int BrNetPacket_1006CDA0_10008D60::m_1006CF80()
{
    return (int)BrBitStreamAtEnd((const struct BrBitStream *)this);
}

/* 0x1006D190: the original calls BrStateGetField10 by address */
BrNetHdr * BrNetPacket_1006CDA0_10008D60::m_1006D190()
{
    return (BrNetHdr *)BrStateGetField10((char *)this);
}
