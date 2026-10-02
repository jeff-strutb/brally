#include "slice1_02.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: sends this car's full state to the other players: it stamps the
 * state into the local eight-entry history for this player (under both the
 * global and the per-player mutex), then builds a packet -- kind byte 0x40
 * mixed with the player id, then the packed state -- and hands it to the
 * sender.  Returns 1 if the packet went out. */
/* @implements 0x10004FD0 glide BrNetSendCarState
 * @cpp_kind method
 * @cpp_symbol ?BrNetSendCarState@@YAHPAX@Z
 *
 * Twin of 0x100051C0 (kind byte 0x40, BrCarStateEncode).  Stack-dtor,
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
/* BrCarStateEncode: prototype in br_funcs.h */
/* 64-bit core: WaitForMultipleObjects is declared by the platform headers */
/* 64-bit core: ReleaseMutex is declared by the platform headers */
}

/* InitPkt: prototype in br_funcs.h */
/* SendPkt was a stand-in; the original calls C function BrNetTrySend.  Declared under
 * its real symbol so the relocation resolves by name. */
/* BrNetTrySend: prototype in br_funcs.h */
#define SendPkt ((int (*)(void *, Pkt *))BrNetTrySend)

int BrNetSendCarState(void *pState)
{
    {
        int      id = g_id;
        void    *h[2];
        NetSlot *pSlot;

        pSlot = &(*(NetSlot (*)[])&g_aBrNetSlot)[id];
        h[0] = g_hBrNetMutex;
        h[1] = pSlot->hMutex;
        WaitForMultipleObjects(2, h, 1, 0xFFFFFFFF);
        g_brNetPktTick = BrTicks30FromMs((struct BrBitStream *)());
        if (++pSlot->idx >= 8)
            pSlot->idx = 0;
        pSlot->stamp[pSlot->idx] = g_brNetPktTick;
        pSlot->kind[pSlot->idx] = 0x40;
        pSlot->data[pSlot->idx] = *(const CarStateBlob *)pState;
        ReleaseMutex(pSlot->hMutex);
        ReleaseMutex(g_hBrNetMutex);
    }
    {
        Pkt pkt;

        BrNetPktStamp(&pkt);
        pkt.PutByte((unsigned char)(g_id | 0x40));
        BrCarStateEncode((struct BrBitStream *)(&pkt), pState);
        if (SendPkt(&g_brP277B40, &pkt) == -1)
            return 0;
        return 1;
    }
}
