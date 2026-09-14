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
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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

typedef char chk_slot[sizeof(NetSlot) == 0x978 ? 1 : -1];
typedef char chk_idx[(unsigned)&((NetSlot *)0)->idx == 0x55C ? 1 : -1];

extern "C" {
volatile int g_id;                  /* 0x1007B264 */
void        *g_hNetMutex;           /* 0x10226A64 */
NetSlot      g_aNetSlot[];          /* 0x1021CE58 */
int          g_netStamp;            /* 0x1021CE40 */
char         g_netTarget;           /* 0x10273328 */
int          BrNetClock_100037D0(void);
int          BrCarStateEncodeDelta(Pkt *, void *, int);
__declspec(dllimport) unsigned long __stdcall WaitForMultipleObjects(unsigned long, void *const *, int, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);
}

void InitPkt(Pkt *);
int SendPkt(void *, Pkt *);

int BrNetSendCarStateDelta(void *pState, int ref)
{
    {
        int      id = g_id;
        void    *h[2];
        NetSlot *pSlot;

        pSlot = &g_aNetSlot[id];
        h[0] = g_hNetMutex;
        h[1] = pSlot->hMutex;
        WaitForMultipleObjects(2, h, 1, 0xFFFFFFFF);
        g_netStamp = BrNetClock_100037D0();
        if (++pSlot->idx >= 8)
            pSlot->idx = 0;
        pSlot->stamp[pSlot->idx] = g_netStamp;
        pSlot->kind[pSlot->idx] = 0x80;
        pSlot->data[pSlot->idx] = *(const CarStateBlob *)pState;
        ReleaseMutex(pSlot->hMutex);
        ReleaseMutex(g_hNetMutex);
    }
    {
        Pkt pkt;

        InitPkt(&pkt);
        pkt.PutByte((unsigned char)(g_id | 0x80));
        BrCarStateEncodeDelta(&pkt, pState, ref);
        if (SendPkt(&g_netTarget, &pkt) == -1)
            return 0;
        return 1;
    }
}
