#include "slice1_02.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: sends this car's state as a difference against an earlier one:
 * the same history stamp and mutex dance as the full-state sender, then a
 * packet of kind 0x80 carrying the delta-encoded state.  Returns 1 if the
 * packet went out. */
/* @implements 0x100051C0 glide BrNetSendCarStateDelta
 * @cpp_kind method
 * @cpp_symbol ?BrNetSendCarStateDelta@@YAHPAXH@Z
 *
 * Twin of 0x10004FD0 (kind byte 0x80, BrCarStateEncodeDelta).  Stack-dtor,
 * maxState=1, unwind `lea ecx,[ebp-0x220]; jmp ~Pkt`, same Pkt class as
 * 0x10004AD0.  BYTE-EXACT 2026-09-13 on the first shape pass (3 probes):
 * the slot pointer is bound BEFORE the two handles are read (the index
 * lea chain precedes the mutex load), the history index advances as a
 * pre-increment inside the test (`if (++idx >= 8)` -- a named n+1 costs
 * a copy and 189 diffs), and the send result is tested `== -1` with the
 * failing return first (VC5 lays the zero arm first and shares the -1
 * between the compare and the EH-state store).  The two-handle array sits
 * in its own scope below the packet; frame 0x21C = 0x214 + 8.
 */
#define _CRTIMP __declspec(dllimport)

class Pkt {
    char b[0x214];
public:
    Pkt();
    ~Pkt();
    void PutByte(unsigned char);
};

typedef char chk_pkt[sizeof(Pkt) == 0x214 ? 1 : -1];

struct CarStateBlob {
    int w[0x28];                    /* 0xA0 bytes */
};

struct NetSlot {
    void        *hMutex;            /* +0x000 */
    int          f004;
    int          f008;
    int          stamp[8];          /* +0x00C */
    int          f02C;
    int          f030;
    int          f034;
    int          kind[8];           /* +0x038 */
    CarStateBlob data[8];           /* +0x058 */
    int          f558;
    int          idx;               /* +0x55C */
    char         pad[0x978 - 0x560];
};




extern "C" {
/* 64-bit core: g_id is defined once, in br_globals.c */
/* 64-bit core: g_hNetMutex is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: g_netStamp is defined once, in br_globals.c */
/* 64-bit core: g_netTarget is defined once, in br_globals.c */
/* BrNetClock_100037D0: prototype in br_funcs.h */
/* BrCarStateEncodeDelta: prototype in br_funcs.h */
/* 64-bit core: WaitForMultipleObjects is declared by the platform headers */
/* 64-bit core: ReleaseMutex is declared by the platform headers */
}

/* InitPkt: prototype in br_funcs.h */
/* SendPkt was a stand-in; the original calls C function BrNetTrySend.  Declared under
 * its real symbol so the relocation resolves by name. */
/* BrNetTrySend: prototype in br_funcs.h */
#define SendPkt ((int (*)(void *, Pkt *))BrNetTrySend)

int BrNetSendCarStateDelta(void *pState, float * ref)
{
    {
        int      id = g_id;
        void    *h[2];
        NetSlot *pSlot;

        pSlot = &(*(NetSlot (*)[])&g_aBrNetSlot)[id];
        h[0] = g_hBrNetMutex;
        h[1] = (*(void * *)&((BrNetSlot *)(pSlot))->hMutex);
        WaitForMultipleObjects(2, h, 1, 0xFFFFFFFF);
        g_brNetPktTick = BrTicks30FromMs();
        if (++(*(int *)&((BrNetSlot *)(pSlot))->f55C) >= 8)
            (*(int *)&((BrNetSlot *)(pSlot))->f55C) = 0;
        (*(int (*)[8])&((BrNetSlot *)(pSlot))->f00C)[(*(int *)&((BrNetSlot *)(pSlot))->f55C)] = g_brNetPktTick;
        (*(int (*)[8])&((BrNetSlot *)(pSlot))->f038)[(*(int *)&((BrNetSlot *)(pSlot))->f55C)] = 0x80;
        (*(CarStateBlob *)&((BrNetSlot *)(pSlot))->cars[pSlot->idx]) = *(const CarStateBlob *)pState;
        ReleaseMutex((*(void * *)&((BrNetSlot *)(pSlot))->hMutex));
        ReleaseMutex(g_hBrNetMutex);
    }
    {
        Pkt pkt;

        BrNetPktStamp(&pkt);
        pkt.PutByte((unsigned char)(g_id | 0x80));
        BrCarStateEncodeDelta((struct BrBitStream *)(&pkt), (const struct BrCarState *)pState, (const struct BrCarState *)ref);
        if (SendPkt(&g_brP277B40, &pkt) == -1)
            return 0;
        return 1;
    }
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x1006CD80: the original calls FUN_1006cd80 by address */
inline Pkt::Pkt()
{
    FUN_1006cd80((int *)this);
}

/* 0x10008D60: the original calls BrPodNop by address */
inline Pkt::~Pkt()
{
    BrPodNop();
}

/* 0x1006CFA0: the original calls BrBitStreamWriteU8 by address */
inline void Pkt::PutByte(unsigned char a1)
{
    BrBitStreamWriteU8((struct BrBitStream *)this, (unsigned int)a1);
}
