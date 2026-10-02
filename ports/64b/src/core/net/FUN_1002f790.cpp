/* WHAT IT DOES: applies one network packet received *from a named remote
 * peer* (idFrom) to the spectator/relay peer table, the sibling of the local
 * table driven by 0x100038F0.  It walks the packet's command stream and for
 * each command, under the target slot's mutex and only while that slot is
 * live and owned by idFrom:
 *   - cmd 0x00: a car header.  With the 0x10 bit set it lands in the slot's
 *     sixteen-entry car-record ring (keyed by the header's sub-slot byte and
 *     only if newer than the record already there): timestamp, flags, the
 *     three status bytes, the id, and the name.  Without the 0x10 bit it is
 *     the slot's own header and only latches the finish/return flags to be
 *     applied once at the end (bStart/bReturn).
 *   - cmd 0x40: a full car-state sample slotted into the eight-entry ring at
 *     the oldest timestamp, decoded in place; the schedule stamp is taken
 *     once, only when the sample is fresh and past the threshold field.
 *   - cmd 0x60: rewind, read the counted length, and skip the block.
 *   - cmd 0x80: a delta sample, lerped between the two newest full samples
 *     (fraction from the surrounding timestamps) and then delta-decoded.
 *   - cmd 0xc0: a timing pair stored on the slot when the 0x10 bit is set.
 *   - cmd 0xe0: with the 0x10 bit clear, a join -- find the peer, reset its
 *     whole slot and its sixteen car records; with the bit set, the status
 *     header (flags and name) for an existing owned slot.
 *   - any other command class (0x20, 0xa0) ends the packet immediately,
 *     skipping the end-of-packet flag pass (the jump table's default edge).
 * After the stream, the latched finish/return flags are OR'd across every
 * live slot, and if the packet did not come from the host (idFrom != 1) and
 * its lead command is a car update, the whole packet is forwarded to the
 * local dispatcher 0x100038F0. */
/* @t3 0x1002F790 2026-09-16 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 2653/2517 insns 864/678 rows 103+289 regions 11 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register allocation/scheduling, behaviour-neutral: the A5 oracle
 * proves same-in/same-out on 48 seeds (peer/record tables, ownership, flag
 * latch, name copy, timing).  The byte gap is the allocation of the two mutex
 * imports and the slot base, plus the switch table counted as code.  Getting
 * the oracle to run this SEH C++ packet-dispatcher class needed real fixes
 * (ret-<imm> esp cleanup, fs: SEH slots, ctor/dtor + EH-handler resolution,
 * call-through-a-cached-import register, valid packet seeding); the last of
 * those was what made the apparent record-path divergence vanish -- it was the
 * emulator leaking a stdcall arg on `call reg`, not this transcription.
 * Do not reopen before the end-grind. */
/* @implements 0x1002F790 glide FUN_1002f790
 * @cpp_kind free
 * @cpp_symbol _FUN_1002f790
 *
 * C++ (confirmed): the EH record carries magic 0x19930520 (__CxxFrameHandler,
 * not C's __except_handler3), maxState 1 -- one destructible stack local, the
 * 0x214-byte BrNetPacket reader, torn down by the unwind nop 0x10008D60.  The
 * five-argument remote-peer twin of 0x100038F0 (four args).  Every read is a
 * thiscall member of that reader.  Drives its own slot table (base 0x117A9B88,
 * stride 0x96C, 16 slots) plus a 256-entry car-record table (base 0x117B3258,
 * same stride, indexed slot*16 + subslot); every field is its own global
 * symbol plus index*0x96C (PF/PA/RF/RA), the idiom the sibling proved.
 *
 * Complete block-by-block transcription of the 2517 B / ~678-insn body: the
 * control flow (the jump-table default->return edge, the shared per-case mutex
 * release blocks reached by goto, the guard branch order) and the read/store
 * order are taken from the disassembly, not from a decompiler draft.
 *
 * @t4-pass 0x1002F790 1 2026-09-16 probes 24 bytes 2653 insns 864 regions 11 rows 392 census no  (byte-exactness grind: name scratch sized to 0x400 to match the frame, index*0x96c hoisted once into soff/roff, imports routed through pointer locals -- MSVC folds them back to call [mem]; register allocation of the two mutex imports and the slot base is the residue, unmoved.)
 * @t4-pass 0x1002F790 2 2026-09-16 probes 11 bytes 2653 insns 864 regions 11 rows 392 census yes  (ordered global-write census: every write to the peer/record tables is identical in address and value to the original, only two record-field stores reordered -- the residue is register allocation/scheduling, not missing or wrong code; the A5 oracle proves same-in/same-out on 48 seeds.  Numbers unmoved from pass 1.)
 */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include <string.h>

struct BrNetHdr {
    int f00;
    int f04;
    int f08;
};

struct BrCarState {
    char raw[0xA0];
};

/* The packet reader.  Methods and the ctor/dtor are named by their VA so the
 * A5 oracle's reloc resolver can map each thiscall to its address (the
 * `?m_<HEX>@` / ctor+dtor `<CTORVA>_<DTORVA>` conventions); this is only the
 * symbol string -- the call bytes (relocations) are identical either way. */
class BrNetPacket_1006CDA0_10008D60 {
public:
    BrNetPacket_1006CDA0_10008D60(void *pBuf, int nBytes);  /* ctor 0x1006CDA0 */
    ~BrNetPacket_1006CDA0_10008D60();                       /* dtor 0x10008D60 (nop) */
    void          m_1006CDD0();             /* Reset */
    void          m_1006CDE0(int n);        /* SkipBytes */
    unsigned char m_1006CE00();             /* ReadU8 */
    int           m_1006CE50();             /* ReadU24 */
    int           m_1006CE80();             /* ReadS32 */
    int           m_1006CF80();             /* AtEnd */
    int           m_1006D180();             /* CountedTotal */
    BrNetHdr     *m_1006D190();             /* GetHdr */

    int            readBit;                 /* +0x00 */
    int            readByte;                /* +0x04 */
    int            writeBit;                /* +0x08 */
    int            writeByte;               /* +0x0C */
    unsigned char *pBuf;                    /* +0x10 */
    unsigned char  payload[0x200];          /* +0x14 */
};
typedef BrNetPacket_1006CDA0_10008D60 BrNetPacket;



extern "C" {
/* 64-bit core: WaitForSingleObject is declared by the platform headers */
/* 64-bit core: ReleaseMutex is declared by the platform headers */

/* Peer/spectator slot table: base 0x117A9B88, stride 0x96C, 16 slots. */
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

/* Car-record table: base 0x117B3258, stride 0x96C, 256 entries. */
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

void  sub_10007230(BrCarState *, BrNetPacket *);                    /* BrCarStateDecode */
void  sub_10007750(BrCarState *, BrCarState *, BrNetPacket *);      /* BrCarStateDecodeDelta */
void  sub_10007D50(BrCarState *, float, BrCarState *, BrCarState *);/* BrCarStateLerp */
unsigned sub_1006A310(void);                                        /* clock */
int   sub_1002F6D0(int id);                                         /* BrPeerFind */
void  sub_100038F0(void *pNet, void *pBuf, int nBytes, int nMode, int a5);
}

/* index*0x96C addressing on a per-field global, as the sibling proved.  The
 * byte offset is computed once into a local (soff/roff) so the compiler keeps
 * the product in one register across the case, as the original holds in esi. */

extern "C" void FUN_1002f790(void *pNet, void *pBuf, int nBytes, int idFrom, int a5)
{
    BrNetPacket pkt(pBuf, nBytes);
    int      bStart = 0;      /* latched cmd-0 0x40 flag */
    int      bReturn = 0;     /* latched cmd-0 0x80 flag */
    unsigned ts;
    unsigned cmd;
    unsigned slot;
    unsigned b10;
    int      soff;
    int      roff;
    int      i;
    char     name[0x400];
    BrCarState scratch;       /* cmd 0x40 stale decode */
    BrCarState scratch2;      /* cmd 0x80 stale decode */

    pkt.m_1006CDD0();
    ts = pkt.m_1006CE50();
    while (!pkt.m_1006CF80()) {
        cmd  = pkt.m_1006CE00();
        b10  = cmd & 0x10;
        slot = cmd & 0xf;
        soff = slot * 0x96c;
        switch (cmd & 0xe0) {
        case 0x00: {
            unsigned      b0    = pkt.m_1006CE00() & 0xff;
            unsigned char flags = pkt.m_1006CE00();
            unsigned      nib   = pkt.m_1006CE00() & 0xff;
            char          ca    = pkt.m_1006CE00();
            char          cb    = pkt.m_1006CE00();
            char          cc    = pkt.m_1006CE00();
            int           id    = pkt.m_1006CE80();
            int           hasName = 0;

            if ((flags & 0x3f) < 3) {
                for (i = 0; i < 0x18; i++)
                    name[i] = pkt.m_1006CE00();
                hasName = i;
                name[0x18] = 0;
            }
            if ((flags & 0x3f) == 4)
                pkt.m_1006CE50();

            WaitForSingleObject(((*(void * *)((char *)&(g_aBrPeer71) + soff))), 0xffffffff);
            if ((((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) & 0x3f) == 0)
                goto c0_rel;
            if (idFrom != ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x4))) + soff))))
                goto c0_rel;
            if (b10 != 0) {
                roff = (slot * 0x10 + b0) * 0x96c;
                WaitForSingleObject(((*(void * *)((char *)&(g_aBr178FEF8) + roff))), 0xffffffff);
                if (ts >= ((*(unsigned *)((char *)&((*(unsigned int *)((char *)&g_aBr178FEF8 + 0x8))) + roff)))) {
                    if (slot == b0) {
                        if (flags & 0x40) {
                            flags &= 0x3f;
                            ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) &= 0xffffff3f;
                        }
                        if (slot == b0 && (flags & 0x80)) {
                            flags &= 0x3f;
                            ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) &= 0xffffff3f;
                        }
                    }
                    ((*(unsigned *)((char *)&((*(unsigned int *)((char *)&g_aBr178FEF8 + 0x8))) + roff))) = ts;
                    ((*(int *)((char *)&((*(int *)((char *)&g_aBr178FEF8 + 0x2C))) + roff)))      = flags;
                    ((*(int *)((char *)&((*(int *)((char *)&g_aBr178FEF8 + 0x30))) + roff)))      = nib;
                    (((char *)((char *)&((*(char *)((char *)&g_aBr178FEF8 + 0x34))) + roff)))[0]  = ca;
                    (((char *)((char *)&((*(char *)((char *)&g_aBr178FEF8 + 0x35))) + roff)))[0]  = cb;
                    (((char *)((char *)&((*(char *)((char *)&g_aBr178FEF8 + 0x36))) + roff)))[0]  = cc;
                    ((*(int *)((char *)&((*(int *)((char *)&g_aBr178FEF8 + 0x4))) + roff)))      = id;
                    if (hasName)
                        strcpy((((char *)((char *)&((*(char *)((char *)&g_aBr178FEF8 + 0x55C))) + roff))), name);
                }
                ReleaseMutex(((*(void * *)((char *)&(g_aBr178FEF8) + roff))));
                ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
                break;
            }
            if (slot == b0) {
                if (flags & 0x40)
                    bStart = 1;
                if (flags & 0x80)
                    bReturn = 1;
                ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) = flags;
            }
        c0_rel:
            ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
            break;
        }

        case 0x40: {
            int      best, cur;
            unsigned bestT;

            WaitForSingleObject(((*(void * *)((char *)&(g_aBrPeer71) + soff))), 0xffffffff);
            if ((((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) & 0x3f) < 2)
                goto c4_rel;
            if (idFrom != ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x4))) + soff))))
                goto c4_rel;
            cur = ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)));
            if (ts <= (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[cur]) {
                sub_10007230(&scratch, &pkt);
                ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
                break;
            }
            best  = 0;
            bestT = 0xffffffff;
            for (i = 0; i < 8; i++) {
                if ((((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i] <= bestT) {
                    best  = i;
                    bestT = (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i];
                }
            }
            ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff))) = best;
            (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[best] = ts;
            (((int *)((char *)&((*(int (*)[])((char *)&g_aBrPeer71 + 0x38))) + soff)))[((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)))] = 0x40;
            sub_10007230(&(((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)))[((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)))], &pkt);
            if (((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x968))) + soff))) != 0)
                goto c4_rel;
            if (*(float *)((char *)&(((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)))[((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)))] + 0x78)
                    < DAT_1007751c)
                goto c4_rel;
            ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x968))) + soff))) = sub_1006A310();
        c4_rel:
            ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
            break;
        }

        case 0x60: {
            int n;

            pkt.m_1006CDD0();
            n = pkt.m_1006D180();
            pkt.m_1006CDE0(n);
            break;
        }

        case 0x80: {
            int      best, iNew, iPrev, d, cur;
            unsigned bestT, tNew, tPrev;
            float    frac;

            WaitForSingleObject(((*(void * *)((char *)&(g_aBrPeer71) + soff))), 0xffffffff);
            if ((((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) & 0x3f) < 2)
                goto c8_rel;
            if (idFrom != ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x4))) + soff))))
                goto c8_rel;
            cur = ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)));
            if (ts <= (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[cur]) {
                sub_10007750(&scratch2, &scratch2, &pkt);
                goto c8_rel;
            }
            best  = 0;
            bestT = 0xffffffff;
            for (i = 0; i < 8; i++) {
                if ((((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i] <= bestT) {
                    bestT = (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i];
                    best  = i;
                }
            }
            iNew = 0;
            tNew = 0;
            for (i = 0; i < 8; i++) {
                if ((((int *)((char *)&((*(int (*)[])((char *)&g_aBrPeer71 + 0x38))) + soff)))[i] == 0x40 && tNew < (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i]) {
                    iNew = i;
                    tNew = (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i];
                }
            }
            iPrev = 0;
            tPrev = 0;
            for (i = 0; i < 8; i++) {
                if ((((int *)((char *)&((*(int (*)[])((char *)&g_aBrPeer71 + 0x38))) + soff)))[i] == 0x40 && tPrev < (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i]
                        && i != iNew) {
                    iPrev = i;
                    tPrev = (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[i];
                }
            }
            d = (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[iNew] - (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[iPrev];
            if (d == 0)
                frac = 1.0f;
            else
                frac = (float)(unsigned)(ts - (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[iPrev]) / d;
            ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff))) = best;
            (((unsigned *)((char *)&((*(unsigned int (*)[])((char *)&g_aBrPeer71 + 0xC))) + soff)))[best] = ts;
            (((int *)((char *)&((*(int (*)[])((char *)&g_aBrPeer71 + 0x38))) + soff)))[((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)))] = 0x80;
            sub_10007D50(&(((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)))[((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)))], frac,
                           &(((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)))[iPrev],
                           &(((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)))[iNew]);
            sub_10007750(&(((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)))[((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff)))],
                                  &(((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)))[iNew], &pkt);
        c8_rel:
            ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
            break;
        }

        case 0xc0: {
            int b   = pkt.m_1006CE50();
            int now = sub_1006A310();

            WaitForSingleObject(((*(void * *)((char *)&(g_aBrPeer71) + soff))), 0xffffffff);
            if ((((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) & 0x3f) != 0 && idFrom == ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x4))) + soff)))
                    && b10 != 0) {
                ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x960))) + soff))) = b;
                ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x964))) + soff))) = now - b;
            }
            ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
            break;
        }

        case 0xe0: {
            unsigned b0  = pkt.m_1006CE00() & 0xff;
            unsigned nib = pkt.m_1006CE00() & 0xff;
            char     ca  = pkt.m_1006CE00();
            char     cb  = pkt.m_1006CE00();
            char     cc  = pkt.m_1006CE00();

            for (i = 0; i < 0x18; i++)
                name[i] = pkt.m_1006CE00();
            name[0x18] = 0;

            if (b10 == 0) {
                int        *pRing;
                BrCarState *pCar;
                int         j, k, m;

                if (DAT_117b3250 != 0)
                    break;
                j = sub_1002F6D0(idFrom);
                if (j == -1)
                    break;
                soff = j * 0x96c;
                WaitForSingleObject(((*(void * *)((char *)&(g_aBrPeer71) + soff))), 0xffffffff);
                ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x4))) + soff))) = idFrom;
                pRing = (((int *)((char *)&((*(int (*)[])((char *)&g_aBrPeer71 + 0x38))) + soff)));
                pCar  = (((BrCarState *)((char *)&((*(BrCarState (*)[])((char *)&g_aBrPeer71 + 0x58))) + soff)));
                k = 8;
                do {
                    pRing[-0xb] = 0;    /* timestamp ring entry (kind ring - 0x2c) */
                    *pRing = 0;         /* kind ring entry */
                    pRing++;
                    memset(pCar, 0, sizeof(BrCarState));
                    pCar++;
                    k--;
                } while (k != 0);
                ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) = 1;
                ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x558))) + soff))) = 0;
                ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x95C))) + soff))) = sub_1006A310();
                roff = j * 0x10 * 0x96c;
                m = 0x10;
                do {
                    WaitForSingleObject(((*(void * *)((char *)&(g_aBr178FEF8) + roff))), 0xffffffff);
                    ((*(unsigned *)((char *)&((*(unsigned int *)((char *)&g_aBr178FEF8 + 0x8))) + roff))) = 0;
                    ((*(int *)((char *)&((*(int *)((char *)&g_aBr178FEF8 + 0x2C))) + roff)))      = 0;
                    ReleaseMutex(((*(void * *)((char *)&(g_aBr178FEF8) + roff))));
                    roff += 0x96c;
                    m--;
                } while (m != 0);
                ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
            } else {
                WaitForSingleObject(((*(void * *)((char *)&(g_aBrPeer71) + soff))), 0xffffffff);
                if ((((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) & 0x3f) != 0 && idFrom == ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x4))) + soff)))
                        && slot == b0) {
                    ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x30))) + soff)))     = nib;
                    (((char *)((char *)&((*(char *)((char *)&g_aBrPeer71 + 0x34))) + soff)))[0] = ca;
                    (((char *)((char *)&((*(char *)((char *)&g_aBrPeer71 + 0x35))) + soff)))[0] = cb;
                    (((char *)((char *)&((*(char *)((char *)&g_aBrPeer71 + 0x36))) + soff)))[0] = cc;
                    strcpy((((char *)((char *)&((*(char *)((char *)&g_aBrPeer71 + 0x55C))) + soff))), name);
                    ((*(int *)((char *)&((*(int *)((char *)&g_aBrPeer71 + 0x2C))) + soff))) = 2;
                }
                ReleaseMutex(((*(void * *)((char *)&(g_aBrPeer71) + soff))));
            }
            break;
        }

        default:
            goto done;
        }
    }

    if (bReturn) {
        int *pst;
        for (pst = &(*(int *)((char *)&g_aBrPeer71 + 0x2C)); (char *)pst < (char *)&(*(int *)((char *)&g_aBrPeer71 + 0x2C)) + 0x10 * 0x96c;
                pst = (int *)((char *)pst + 0x96c)) {
            void    *h = *(void **)((char *)pst - 0x2c);
            unsigned s;

            WaitForSingleObject(h, 0xffffffff);
            s = *pst & 0x3f;
            if (s > 1 && s < 5)
                *pst = (*pst & 0xffffffbf) | 0x80;
            ReleaseMutex(h);
        }
    }
    if (bStart) {
        int *pst;
        for (pst = &(*(int *)((char *)&g_aBrPeer71 + 0x2C)); (char *)pst < (char *)&(*(int *)((char *)&g_aBrPeer71 + 0x2C)) + 0x10 * 0x96c;
                pst = (int *)((char *)pst + 0x96c)) {
            void    *h = *(void **)((char *)pst - 0x2c);
            unsigned s;

            WaitForSingleObject(h, 0xffffffff);
            s = *pst & 0x3f;
            if (s > 1 && s < 5)
                *pst = (*pst & 0xffffff7f) | 0x40;
            ReleaseMutex(h);
        }
    }

    if (idFrom != 1) {
        unsigned char lead = ((unsigned char *)pkt.m_1006D190())[3] & 0xe0;
        if (lead == 0x40 || lead == 0x80 || lead == 0x60)
            sub_100038F0(pNet, pBuf, nBytes, idFrom, a5);
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

/* 0x1006CDE0: the original calls BrBitStreamSkipBytes by address */
void BrNetPacket_1006CDA0_10008D60::m_1006CDE0(int a1)
{
    BrBitStreamSkipBytes((struct BrBitStream *)this, (int)a1);
}

/* 0x1006CE00: the original calls BrBitStreamReadU8 by address */
unsigned char BrNetPacket_1006CDA0_10008D60::m_1006CE00()
{
    return (unsigned char)BrBitStreamReadU8((struct BrBitStream *)this);
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

/* 0x1006D180: the original calls BrCountedTotal by address */
int BrNetPacket_1006CDA0_10008D60::m_1006D180()
{
    return (int)BrCountedTotal((const struct BrCounted *)this);
}

/* 0x1006D190: the original calls BrStateGetField10 by address */
BrNetHdr * BrNetPacket_1006CDA0_10008D60::m_1006D190()
{
    return (BrNetHdr *)BrStateGetField10((char *)this);
}
