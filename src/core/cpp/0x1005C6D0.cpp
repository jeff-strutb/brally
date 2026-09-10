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
 * PARKED at 12 positional diffs on 468 real bytes (the 480 recomp is 12
 * trailing nops).  Everything is byte-exact except ONE six-instruction
 * window: the three tail copies +0x27B0..B8 -> +0x28EC..F4.  The original
 * emits them as per-statement load/store PAIRS rotating edx/eax/ecx; ours
 * batches the three loads then the three stores (same registers, same
 * roles).  DEAD: int fields, float fields, array-element spelling, and
 * *(int*)((char*)this+off) casts all batch (VC5 disambiguates same-base
 * constant offsets regardless of spelling); a 12-byte struct assignment
 * changes the copy shape entirely (-4 B); /Op adds a spill slot at the
 * prologue; /GX on/off identical; fF78=1 hoisted above the copies is
 * worse (29).  The load arm of the mode test MUST be spelled as two
 * whole SetPos calls (if/else around the call, not around locals): VC5
 * hoists the common z argument above the branch and cross-jumps the tail,
 * which is where 322 of the original 398 diffs went.
 * @t4-pass 2026-09-09 probes=10 result=diff12/tail-pair-window census no
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

/* One wheel record, seen through the three slots this method touches. */
class Whl5C6D0 {
public:
    char          pad0000[0x19C];
    int           f19C;                 /* +0x19C */
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

typedef char chk_38[(unsigned)&((Car5C6D0 *)0)->f38   == 0x38   ? 1 : -1];
typedef char chk_68[(unsigned)&((Car5C6D0 *)0)->p168  == 0x168  ? 1 : -1];
typedef char chk_5c[(unsigned)&((Car5C6D0 *)0)->f35C  == 0x35C  ? 1 : -1];
typedef char chk_a0[(unsigned)&((Car5C6D0 *)0)->fEA0  == 0xEA0  ? 1 : -1];
typedef char chk_78[(unsigned)&((Car5C6D0 *)0)->fF78  == 0xF78  ? 1 : -1];
typedef char chk_8c[(unsigned)&((Car5C6D0 *)0)->fF8C  == 0xF8C  ? 1 : -1];
typedef char chk_80[(unsigned)&((Car5C6D0 *)0)->f2780 == 0x2780 ? 1 : -1];
typedef char chk_b0[(unsigned)&((Car5C6D0 *)0)->f27B0 == 0x27B0 ? 1 : -1];
typedef char chk_ec[(unsigned)&((Car5C6D0 *)0)->f28EC == 0x28EC ? 1 : -1];
typedef char chk_af[(unsigned)&((Car5C6D0 *)0)->b29AF == 0x29AF ? 1 : -1];
typedef char chk_w9[(unsigned)&((Whl5C6D0 *)0)->f19C  == 0x19C  ? 1 : -1];
typedef char chk_wa[(unsigned)&((Whl5C6D0 *)0)->b1A0  == 0x1A0  ? 1 : -1];
typedef char chk_wb[(unsigned)&((Whl5C6D0 *)0)->f1B4  == 0x1B4  ? 1 : -1];

extern "C" {
extern float DAT_106eed10;              /* 0x106EED10 current race time  */
extern float DAT_10077898;              /* 0x10077898 reset window       */
extern float DAT_1007789c;              /* 0x1007789C node height drop   */
extern float DAT_100778a0;              /* 0x100778A0 per-lap height     */
extern int   DAT_100a9360;              /* 0x100A9360 game mode          */
float BrAtan2_10034E30(float x, float y);   /* 0x10034E30, cdecl         */
}

void Car5C6D0::Respawn()
{
    if ((f35C < 0) || (f38 < DAT_106eed10 - DAT_10077898)) {
        Sub5E6A0();
        Sub5BCC0();
        if (DAT_100a9360 == 2)
            SetPos(*(float *)(fF8C + fF90 * 0x28 + 0x4C),
                   *(float *)(fF8C + (fF90 + 2) * 0x28),
                   *(float *)(fF8C + 0x54 + fF90 * 0x28) - DAT_1007789c);
        else
            SetPos(*(float *)(fF8C + fF90 * 0x28 + 0x4C)
                       - (float)f140 * DAT_100778a0,
                   *(float *)(fF8C + (fF90 + 2) * 0x28),
                   *(float *)(fF8C + 0x54 + fF90 * 0x28) - DAT_1007789c);
        SetHeading((float)BrAtan2_10034E30(fF94, fF98));
        SetVel(0.0f, 0.0f, 0.0f);
        SetAngVel(0.0f, 0.0f, 0.0f);
        p168->f19C = 0;
        p168->f1B4 = 0;
        p168->b1A0 = 2;
        p16C->f19C = 0;
        p16C->f1B4 = 0;
        p16C->b1A0 = 2;
        p174->f19C = 0;
        p174->f1B4 = 0;
        p174->b1A0 = 2;
        p170->f19C = 0;
        p170->f1B4 = 0;
        p170->b1A0 = 2;
        fEA0 = 0.0f;
        fEA4 = 0.0f;
        fEA8 = 0.0f;
        fEAC = 0xffffff4c;
        f35C = 0;
        b29AF = 2;
        f29B0 = 0x3dcccccd;
        Chase(f2780, 1.0f);
        f28EC[0] = f27B0[0];
        f28EC[1] = f27B0[1];
        f28EC[2] = f27B0[2];
        fF78 = 1;
    }
}
