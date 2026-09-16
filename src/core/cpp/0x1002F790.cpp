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
 * After the stream, the latched finish/return flags are OR'd across every
 * live slot, and if the packet did not come from the host (idFrom != 1) and
 * its lead command is a car update, the whole packet is forwarded to the
 * local dispatcher 0x100038F0. */
/* @implements 0x1002F790 glide FUN_1002f790
 * @cpp_kind free
 * @cpp_symbol ?FUN_1002f790@@YAXPAX0HHH@Z
 *
 * 2517 B cdecl EH-frame packet applier, the five-argument remote-peer twin
 * of 0x100038F0 (four args).  Same 0x214-byte BrNetPacket reader constructed
 * on the frame with a THISCALL ctor (ret 8) and torn down by the unwind nop
 * 0x10008D60; every read is a thiscall member, which routes it to the C++
 * lane exactly as the sibling and the car-state codec pair.  It drives a
 * DIFFERENT slot table from the sibling (base 0x117A9B88, stride 0x96C, 16
 * slots) plus a 256-entry car-record table (base 0x117B3258, same stride,
 * indexed slot*16 + subslot); every field is its own global symbol plus
 * index*0x96C (PF/PA/RF/RA), the idiom the sibling proved.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

struct BrNetHdr {
    int f00;
    int f04;
    int f08;
};

struct BrCarState {
    char raw[0xA0];
};

/* The packet reader, identical to the sibling's BrNetPacket. */
class BrNetPacket {
public:
    BrNetPacket(void *pBuf, int nBytes);    /* 0x1006CDA0 */
    ~BrNetPacket();                         /* 0x10008D60 (nop) */
    void          Reset();                  /* 0x1006CDD0 */
    void          SkipBytes(int n);         /* 0x1006CDE0 */
    unsigned char ReadU8();                 /* 0x1006CE00 */
    int           ReadU24();                /* 0x1006CE50 */
    int           ReadS32();                /* 0x1006CE80 */
    int           AtEnd();                  /* 0x1006CF80 */
    int           CountedTotal();           /* 0x1006D180 */
    BrNetHdr     *GetHdr();                 /* 0x1006D190 */

    int            readBit;                 /* +0x00 */
    int            readByte;                /* +0x04 */
    int            writeBit;                /* +0x08 */
    int            writeByte;               /* +0x0C */
    unsigned char *pBuf;                    /* +0x10 */
    unsigned char  payload[0x200];          /* +0x14 */
};

typedef char chk_pkt[sizeof(BrNetPacket) == 0x214 ? 1 : -1];

extern "C" {
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

/* Peer/spectator slot table: base 0x117A9B88, stride 0x96C, 16 slots. */
extern void      *DAT_117a9b88;          /* +0x000  hMutex          */
extern int        DAT_117a9b8c;          /* +0x004  owner id        */
extern unsigned   DAT_117a9b94[];        /* +0x00C  timestamp ring  */
extern int        DAT_117a9bb4;          /* +0x02C  state/flags     */
extern int        DAT_117a9bb8;          /* +0x030                  */
extern char       DAT_117a9bbc;          /* +0x034  three bytes     */
extern char       DAT_117a9bbd;          /* +0x035                  */
extern char       DAT_117a9bbe;          /* +0x036                  */
extern int        DAT_117a9bc0[];        /* +0x038  kind ring       */
extern BrCarState DAT_117a9be0[];        /* +0x058  car ring [8]    */
extern int        DAT_117aa0e0;          /* +0x558  current index   */
extern char       DAT_117aa0e4;          /* +0x55C  name            */
extern int        DAT_117aa4e4;          /* +0x95C                  */
extern int        DAT_117aa4e8;          /* +0x960                  */
extern int        DAT_117aa4ec;          /* +0x964                  */
extern int        DAT_117aa4f0;          /* +0x968  schedule stamp  */

/* Car-record table: base 0x117B3258, stride 0x96C, 256 entries. */
extern void      *DAT_117b3258;          /* +0x000  hMutex          */
extern int        DAT_117b325c;          /* +0x004  id              */
extern unsigned   DAT_117b3260;          /* +0x008  timestamp       */
extern int        DAT_117b3284;          /* +0x02C  flags           */
extern int        DAT_117b3288;          /* +0x030  status byte     */
extern char       DAT_117b328c;          /* +0x034  three bytes     */
extern char       DAT_117b328d;          /* +0x035                  */
extern char       DAT_117b328e;          /* +0x036                  */
extern char       DAT_117b37b4;          /* +0x55C  name            */

extern int        DAT_117b3250;          /* standalone join gate    */
extern float      DAT_1007751c;          /* freshness threshold     */

void  BrCarStateDecode(BrCarState *, BrNetPacket *);                    /* 0x10007230 */
void  BrCarStateDecodeDelta(BrCarState *, BrCarState *, BrNetPacket *); /* 0x10007750 */
void  BrCarStateLerp(BrCarState *, float, BrCarState *, BrCarState *);  /* 0x10007D50 */
unsigned FUN_1006a310(void);                                           /* clock */
int   FUN_1002f6d0(int id);                                            /* BrPeerFind */
void  FUN_100038f0(void *pNet, void *pBuf, int nBytes, int nMode, int a5);
}

/* index*0x96C addressing on a per-field global, as the sibling proved.  The
 * byte offset is computed once into a local (soff/roff) so the compiler keeps
 * the product in one register across the case, as the original does in esi. */
#define PF(sym, T) (*(T *)((char *)&sym + soff))
#define PA(sym, T) ((T *)((char *)&sym + soff))
#define RF(sym, T) (*(T *)((char *)&sym + roff))
#define RA(sym, T) ((T *)((char *)&sym + roff))

void FUN_1002f790(void *pNet, void *pBuf, int nBytes, int idFrom, int a5)
{
    BrNetPacket pkt(pBuf, nBytes);
    int          bReturn = 0;    /* latched 0x80 flag */
    int          bStart = 0;     /* latched 0x40 flag */
    unsigned     ts;
    unsigned     cmd;
    unsigned     b10;            /* command's 0x10 bit */
    unsigned     slot;           /* command's low nibble */
    int          soff;           /* slot * 0x96c, held across the case */
    int          roff;           /* record index * 0x96c */
    int          i;
    char         name[0x400];
    BrCarState   scratch;
    BrCarState   scratch2;

    pkt.Reset();
    ts = pkt.ReadU24();
    while (!pkt.AtEnd()) {
        cmd  = pkt.ReadU8();
        b10  = cmd & 0x10;
        slot = cmd & 0xf;
        soff = slot * 0x96c;
        switch (cmd & 0xe0) {
        case 0x00: {
            unsigned      b0    = pkt.ReadU8() & 0xff;
            unsigned char flags = pkt.ReadU8();
            unsigned      nib   = pkt.ReadU8() & 0xff;
            char          ca    = pkt.ReadU8();
            char          cb    = pkt.ReadU8();
            char          cc    = pkt.ReadU8();
            int           id    = pkt.ReadS32();
            int           hasName = 0;

            if ((flags & 0x3f) < 3) {
                for (i = 0; i < 0x18; i++)
                    name[i] = pkt.ReadU8();
                name[0x18] = 0;
                hasName = i;
            }
            if ((flags & 0x3f) == 4)
                pkt.ReadU24();

            WaitForSingleObject(PF(DAT_117a9b88, void *), 0xffffffff);
            if ((PF(DAT_117a9bb4, int) & 0x3f) != 0 && idFrom == PF(DAT_117a9b8c, int)) {
                if (b10 != 0) {
                    roff = (slot * 0x10 + b0) * 0x96c;

                    WaitForSingleObject(RF(DAT_117b3258, void *), 0xffffffff);
                    if (RF(DAT_117b3260, unsigned) <= ts) {
                        if (slot == b0) {
                            if (flags & 0x40) {
                                flags &= 0x3f;
                                PF(DAT_117a9bb4, int) &= 0xffffff3f;
                            }
                            if (slot == b0 && (flags & 0x80)) {
                                flags &= 0x3f;
                                PF(DAT_117a9bb4, int) &= 0xffffff3f;
                            }
                        }
                        RF(DAT_117b3260, unsigned) = ts;
                        RF(DAT_117b3284, int)      = flags;
                        RF(DAT_117b3288, int)      = nib;
                        RA(DAT_117b328c, char)[0]  = ca;
                        RA(DAT_117b328d, char)[0]  = cb;
                        RA(DAT_117b328e, char)[0]  = cc;
                        RF(DAT_117b325c, int)      = id;
                        if (hasName)
                            strcpy(RA(DAT_117b37b4, char), name);
                    }
                    ReleaseMutex(RF(DAT_117b3258, void *));
                    ReleaseMutex(PF(DAT_117a9b88, void *));
                    break;
                }
                if (slot == b0) {
                    if (flags & 0x40)
                        bStart = 1;
                    if (flags & 0x80)
                        bReturn = 1;
                    PF(DAT_117a9bb4, int) = flags;
                }
            }
            ReleaseMutex(PF(DAT_117a9b88, void *));
            break;
        }

        case 0x40: {
            int      best;
            unsigned bestT;

            WaitForSingleObject(PF(DAT_117a9b88, void *), 0xffffffff);
            if ((PF(DAT_117a9bb4, int) & 0x3f) < 2 || idFrom != PF(DAT_117a9b8c, int)) {
                ReleaseMutex(PF(DAT_117a9b88, void *));
                break;
            }
            if (ts <= PA(DAT_117a9b94, unsigned)[PF(DAT_117aa0e0, int)]) {
                BrCarStateDecode(&scratch, &pkt);
                ReleaseMutex(PF(DAT_117a9b88, void *));
                break;
            }
            best  = 0;
            bestT = 0xffffffff;
            for (i = 0; i < 8; i++) {
                if (PA(DAT_117a9b94, unsigned)[i] <= bestT) {
                    best  = i;
                    bestT = PA(DAT_117a9b94, unsigned)[i];
                }
            }
            PF(DAT_117aa0e0, int) = best;
            PA(DAT_117a9b94, unsigned)[best] = ts;
            PA(DAT_117a9bc0, int)[PF(DAT_117aa0e0, int)] = 0x40;
            BrCarStateDecode(&PA(DAT_117a9be0, BrCarState)[PF(DAT_117aa0e0, int)], &pkt);
            if (PF(DAT_117aa4f0, int) != 0) {
                ReleaseMutex(PF(DAT_117a9b88, void *));
                break;
            }
            if (*(float *)((char *)&PA(DAT_117a9be0, BrCarState)[PF(DAT_117aa0e0, int)] + 0x78)
                    < DAT_1007751c) {
                ReleaseMutex(PF(DAT_117a9b88, void *));
                break;
            }
            PF(DAT_117aa4f0, int) = FUN_1006a310();
            ReleaseMutex(PF(DAT_117a9b88, void *));
            break;
        }

        case 0x60: {
            int n;

            pkt.Reset();
            n = pkt.CountedTotal();
            pkt.SkipBytes(n);
            break;
        }

        case 0x80: {
            int      best, iNew, iPrev, d;
            unsigned bestT, tNew, tPrev;
            float    frac;

            WaitForSingleObject(PF(DAT_117a9b88, void *), 0xffffffff);
            if ((PF(DAT_117a9bb4, int) & 0x3f) >= 2 && idFrom == PF(DAT_117a9b8c, int)) {
                if (ts > PA(DAT_117a9b94, unsigned)[PF(DAT_117aa0e0, int)]) {
                    best  = 0;
                    bestT = 0xffffffff;
                    for (i = 0; i < 8; i++) {
                        if (PA(DAT_117a9b94, unsigned)[i] <= bestT) {
                            bestT = PA(DAT_117a9b94, unsigned)[i];
                            best  = i;
                        }
                    }
                    iNew = 0;
                    tNew = 0;
                    for (i = 0; i < 8; i++) {
                        if (PA(DAT_117a9bc0, int)[i] == 0x40 && tNew < PA(DAT_117a9b94, unsigned)[i]) {
                            iNew = i;
                            tNew = PA(DAT_117a9b94, unsigned)[i];
                        }
                    }
                    iPrev = 0;
                    tPrev = 0;
                    for (i = 0; i < 8; i++) {
                        if (PA(DAT_117a9bc0, int)[i] == 0x40 && tPrev < PA(DAT_117a9b94, unsigned)[i]
                                && i != iNew) {
                            iPrev = i;
                            tPrev = PA(DAT_117a9b94, unsigned)[i];
                        }
                    }
                    d = PA(DAT_117a9b94, unsigned)[iNew] - PA(DAT_117a9b94, unsigned)[iPrev];
                    if (d == 0)
                        frac = 1.0f;
                    else
                        frac = (float)(unsigned)(ts - PA(DAT_117a9b94, unsigned)[iPrev]) / d;
                    PF(DAT_117aa0e0, int) = best;
                    PA(DAT_117a9b94, unsigned)[best] = ts;
                    PA(DAT_117a9bc0, int)[PF(DAT_117aa0e0, int)] = 0x80;
                    BrCarStateLerp(&PA(DAT_117a9be0, BrCarState)[PF(DAT_117aa0e0, int)], frac,
                                   &PA(DAT_117a9be0, BrCarState)[iPrev],
                                   &PA(DAT_117a9be0, BrCarState)[iNew]);
                    BrCarStateDecodeDelta(&PA(DAT_117a9be0, BrCarState)[PF(DAT_117aa0e0, int)],
                                          &PA(DAT_117a9be0, BrCarState)[iNew], &pkt);
                } else {
                    BrCarStateDecodeDelta(&scratch, &scratch, &pkt);
                }
            }
            ReleaseMutex(PF(DAT_117a9b88, void *));
            break;
        }

        case 0xc0: {
            int b = pkt.ReadU24();
            int now = FUN_1006a310();

            WaitForSingleObject(PF(DAT_117a9b88, void *), 0xffffffff);
            if ((PF(DAT_117a9bb4, int) & 0x3f) != 0 && idFrom == PF(DAT_117a9b8c, int) && b10 != 0) {
                PF(DAT_117aa4e8, int) = b;
                PF(DAT_117aa4ec, int) = now - b;
            }
            ReleaseMutex(PF(DAT_117a9b88, void *));
            break;
        }

        case 0xe0: {
            unsigned b0    = pkt.ReadU8() & 0xff;
            unsigned nib   = pkt.ReadU8() & 0xff;
            char     ca    = pkt.ReadU8();
            char     cb    = pkt.ReadU8();
            char     cc    = pkt.ReadU8();
            int      hasName = 0;

            for (i = 0; i < 0x18; i++)
                name[i] = pkt.ReadU8();
            name[0x18] = 0;

            if (b10 == 0) {
                int j;

                if (DAT_117b3250 != 0)
                    break;
                j = FUN_1002f6d0(idFrom);
                if (j == -1)
                    break;
                {
                    int  *pRing;
                    BrCarState *pCar;
                    int   k;

                    soff = j * 0x96c;            /* PF/PA now point at slot j */
                    WaitForSingleObject(PF(DAT_117a9b88, void *), 0xffffffff);
                    PF(DAT_117a9b8c, int) = idFrom;
                    pRing = PA(DAT_117a9bc0, int);
                    pCar  = PA(DAT_117a9be0, BrCarState);
                    k = 8;
                    do {
                        pRing[-0xb] = 0;         /* timestamp ring entry */
                        *pRing = 0;              /* kind ring entry      */
                        pRing++;
                        memset(pCar, 0, sizeof(BrCarState));
                        pCar++;
                        k--;
                    } while (k != 0);
                    PF(DAT_117a9bb4, int) = 1;
                    PF(DAT_117aa0e0, int) = 0;
                    PF(DAT_117aa4e4, int) = FUN_1006a310();
                    {
                        int m = 0x10;
                        roff = j * 0x10 * 0x96c;
                        do {
                            WaitForSingleObject(RF(DAT_117b3258, void *), 0xffffffff);
                            RF(DAT_117b3260, unsigned) = 0;
                            RF(DAT_117b3284, int)      = 0;
                            ReleaseMutex(RF(DAT_117b3258, void *));
                            roff += 0x96c;
                            m--;
                        } while (m != 0);
                    }
                    ReleaseMutex(PF(DAT_117a9b88, void *));
                }
            } else {
                WaitForSingleObject(PF(DAT_117a9b88, void *), 0xffffffff);
                if ((PF(DAT_117a9bb4, int) & 0x3f) != 0 && idFrom == PF(DAT_117a9b8c, int)
                        && slot == b0) {
                    PF(DAT_117a9bb8, int)     = nib;
                    PA(DAT_117a9bbc, char)[0] = ca;
                    PA(DAT_117a9bbd, char)[0] = cb;
                    PA(DAT_117a9bbe, char)[0] = cc;
                    strcpy(PA(DAT_117aa0e4, char), name);
                    PF(DAT_117a9bb4, int) = 2;
                }
                ReleaseMutex(PF(DAT_117a9b88, void *));
            }
            break;
        }
        }
    }

    if (bReturn) {
        int *pst;
        for (pst = &DAT_117a9bb4; (char *)pst < (char *)&DAT_117a9bb4 + 0x10 * 0x96c;
                pst = (int *)((char *)pst + 0x96c)) {
            void *h = *(void **)((char *)pst - 0x2c);
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
        for (pst = &DAT_117a9bb4; (char *)pst < (char *)&DAT_117a9bb4 + 0x10 * 0x96c;
                pst = (int *)((char *)pst + 0x96c)) {
            void *h = *(void **)((char *)pst - 0x2c);
            unsigned s;

            WaitForSingleObject(h, 0xffffffff);
            s = *pst & 0x3f;
            if (s > 1 && s < 5)
                *pst = (*pst & 0xffffff7f) | 0x40;
            ReleaseMutex(h);
        }
    }

    if (idFrom != 1) {
        unsigned char lead = ((unsigned char *)pkt.GetHdr())[3] & 0xe0;
        if (lead == 0x40 || lead == 0x80 || lead == 0x60)
            FUN_100038f0(pNet, pBuf, nBytes, idFrom, a5);
    }
}
