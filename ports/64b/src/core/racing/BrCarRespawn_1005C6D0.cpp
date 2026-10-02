#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_44.h"   /* BrRbBody, the canonical record */
#include "slice3_41.h"   /* BrDriverCar, the canonical record */
/* WHAT IT DOES: respawn the car at its current track node when it has been
 * flagged for reset (+0x35C negative) or its last-progress timestamp (+0x38)
 * has fallen behind the reset window.  Re-inits the car tables, snaps the
 * entity onto the node row of the +0xF8C table (mode 2 uses the row height
 * as-is, other modes drop it by lap-count times a constant), points it at
 * the next node with atan2, zeroes velocity and angular velocity, resets the
 * four wheel records (+0x19C/+0x1B4 cleared, state byte +0x1A0 = 2), clears
 * the impulse block +0xEA0 (with +0xEAC = -180), re-arms the reset flag and
 * steering ramp, re-places the chase camera at unity blend, copies the
 * camera frame position into the +0x28EC mirror and raises +0xF78. */
/* @implements 0x1005C6D0 glide BrCarRespawn_1005C6D0
 * @cpp_kind method
 * @cpp_symbol ?Respawn@Car5C6D0@@QAEXXZ
 *
 * Thiscall, no stack args (`ret`), 468 B.  Every callee is a thiscall
 * method on the same object (the C lane cannot spell those call sites
 * without an edx write), plus one cdecl atan2 whose x87 result is fstp'd
 * straight into the heading setter's argument slot.
 *
 * BYTE-EXACT 2026-09-13 (13 cpp probes on top of the 10 of 09-09).  The
 * three tail copies +0x27B0..B8 -> +0x28EC..F4 go through two POINTER
 * locals (`d = f28EC; s = f27B0; d[0] = s[0]; ...`): with the arrays
 * addressed through pointers VC5 emits per-statement load/store PAIRS
 * rotating edx/eax/ecx, which is the original's window; addressed as
 * members (int or float fields, array elements, casts, a temp per copy,
 * reversed order) it batches the three loads then the three stores
 * (12 diffs).  The load arm of the mode test MUST be spelled as two
 * whole SetPos calls (if/else around the call, not around locals): VC5
 * hoists the common z argument above the branch and cross-jumps the tail,
 * which is where 322 of the original 398 diffs went.
 * @t4-pass 2026-09-09 probes=10 result=diff12/tail-pair-window census no
 */
#define _CRTIMP __declspec(dllimport)

/* One wheel record, seen through the three slots this method touches. */
class Whl5C6D0 {
public:
    char          pad0000[0x19C];
    void         *pPlane;               /* +0x19C */
    unsigned char b1A0;                 /* +0x1A0 */
    char          pad01A1[0x1B4 - 0x1A1];
    int           f1B4;                 /* +0x1B4 */
};

class Car5C6D0 {
public:
    char       pad0000[0x38];
    float      f38;                     /* +0x038 last-progress time     */
    char       pad003C[0x140 - 0x3C];
    int        f140;                    /* +0x140 lap count              */
    char       pad0144[0x168 - 0x144];
    Whl5C6D0  *p168;                    /* +0x168 wheel                  */
    Whl5C6D0  *p16C;                    /* +0x16C wheel                  */
    Whl5C6D0  *p170;                    /* +0x170 wheel                  */
    Whl5C6D0  *p174;                    /* +0x174 wheel                  */
    char       pad0178[0x35C - 0x178];
    int        f35C;                    /* +0x35C reset flag             */
    char       pad0360[0xEA0 - 0x360];
    float      fEA0;                    /* +0xEA0 impulse block          */
    float      fEA4;
    float      fEA8;
    int        fEAC;                    /* +0xEAC gets -180              */
    char       pad0EB0[0xF78 - 0xEB0];
    int        fF78;                    /* +0xF78 raised at the end      */
    char       pad0F7C[0xF8C - 0xF7C];
    int        fF8C;                    /* +0xF8C node table base        */
    int        fF90;                    /* +0xF90 node index             */
    float      fF94;                    /* +0xF94 heading dx             */
    float      fF98;                    /* +0xF98 heading dz             */
    char       pad0F9C[0x2780 - 0xF9C];
    float      f2780[3];                /* +0x2780 camera anchor         */
    char       pad278C[0x27B0 - 0x278C];
    float      f27B0[3];                /* +0x27B0 camera frame pos      */
    char       pad27BC[0x28EC - 0x27BC];
    float      f28EC[3];                /* +0x28EC mirror of +0x27B0     */
    char       pad28F8[0x29AF - 0x28F8];
    unsigned char b29AF;                /* +0x29AF steering state = 2    */
    int        f29B0;                   /* +0x29B0 steering ramp = 0.1f  */

    void Respawn();                     /* 0x1005C6D0 -- defined here    */
    void Sub5E6A0();                    /* 0x1005E6A0 BrCarInitTables    */
    void Sub5BCC0();                    /* 0x1005BCC0                    */
    void SetPos(float x, float y, float z); /* 0x1006F680                */
    void SetHeading(float a);           /* 0x1006F720                    */
    void SetVel(float x, float y, float z);     /* 0x1006FA10            */
    void SetAngVel(float x, float y, float z);  /* 0x1006FC10            */
    void Chase(float *pAnchor, float fBlend);   /* 0x100018F0            */
};










extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrAtan2_10034E30: prototype in br_funcs.h */
}

void Car5C6D0::Respawn()
{
    float *d;
    float *s;

    if (((*(int *)&((BrDriverCar *)(this))->aBody[0].f01F8) < 0) || ((*(float *)&((BrDriverCar *)(this))->pos.z) < g_brTrkHdr.fZMin - DAT_10077898)) {
        Sub5E6A0();
        Sub5BCC0();
        if ((*(int *)&g_brRaceRules.mode) == 2)
            SetPos(*(float *)(((intptr_t)((BrDriverCar *)(this))->pNode.p) + (*(int *)&((BrDriverCar *)(this))->iPt.v) * 0x28 + 0x4C),
                   *(float *)(((intptr_t)((BrDriverCar *)(this))->pNode.p) + ((*(int *)&((BrDriverCar *)(this))->iPt.v) + 2) * 0x28),
                   *(float *)(((intptr_t)((BrDriverCar *)(this))->pNode.p) + 0x54 + (*(int *)&((BrDriverCar *)(this))->iPt.v) * 0x28) - DAT_1007789c);
        else
            SetPos(*(float *)(((intptr_t)((BrDriverCar *)(this))->pNode.p) + (*(int *)&((BrDriverCar *)(this))->iPt.v) * 0x28 + 0x4C)
                       - (float)(*(int *)&((BrDriverCar *)(this))->f140) * DAT_100778a0,
                   *(float *)(((intptr_t)((BrDriverCar *)(this))->pNode.p) + ((*(int *)&((BrDriverCar *)(this))->iPt.v) + 2) * 0x28),
                   *(float *)(((intptr_t)((BrDriverCar *)(this))->pNode.p) + 0x54 + (*(int *)&((BrDriverCar *)(this))->iPt.v) * 0x28) - DAT_1007789c);
        SetHeading((float)BrAtan2((*(float *)&((BrDriverCar *)(this))->f0F94), (*(float *)&((BrDriverCar *)(this))->f0F98)));
        SetVel(0.0f, 0.0f, 0.0f);
        SetAngVel(0.0f, 0.0f, 0.0f);
        (*(void * *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[0])))->pPlane) = 0;
        (*(int *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[0])))->f1B4) = 0;
        (*(unsigned char *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[0])))->f01A0) = 2;
        (*(void * *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[1])))->pPlane) = 0;
        (*(int *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[1])))->f1B4) = 0;
        (*(unsigned char *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[1])))->f01A0) = 2;
        (*(void * *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[3])))->pPlane) = 0;
        (*(int *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[3])))->f1B4) = 0;
        (*(unsigned char *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[3])))->f01A0) = 2;
        (*(void * *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[2])))->pPlane) = 0;
        (*(int *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[2])))->f1B4) = 0;
        (*(unsigned char *)&((BrRbBody *)((*(Whl5C6D0 * *)&((BrDriverCar *)(this))->aBody[0].rb.child[2])))->f01A0) = 2;
        (*(float *)&((BrDriverCar *)(this))->cHoldFwd) = 0.0f;
        (*(float *)&((BrDriverCar *)(this))->cHoldRev) = 0.0f;
        (*(float *)&((BrDriverCar *)(this))->cRevRun) = 0.0f;
        (*(int *)&((BrDriverCar *)(this))->cFwdRun) = 0xffffff4c;
        (*(int *)&((BrDriverCar *)(this))->aBody[0].f01F8) = 0;
        (*(unsigned char *)&((BrDriverCar *)(this))->b29AF) = 2;
        (*(int *)&((BrDriverCar *)(this))->f29B0) = 0x3dcccccd;
        Chase((*(float (*)[3])&((BrDriverCar *)(this))->aSnap[1].m[0][0]), 1.0f);
        d = (*(float (*)[3])&((BrDriverCar *)(this))->f28EC[0]);
        s = (*(float (*)[3])&((BrDriverCar *)(this))->aSnap[1].m[3][0]);
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        (*(int *)&((BrDriverCar *)(this))->fF78) = 1;
    }
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" void BrCarRespawn_1005C6D0(void *self)
{
    ((class Car5C6D0 *)self)->Respawn();
}
/* end of C entry points */
