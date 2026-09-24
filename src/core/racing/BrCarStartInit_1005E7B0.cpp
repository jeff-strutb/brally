/* WHAT IT DOES: the entrant's start-of-race pass on a freshly constructed
 * car: resets the entity (0x1006FD90) and binds it to its grid slot
 * (0x1006FCB0) by lap count, works out the row/column offset for that slot
 * (row 0.5 in single-row modes, else a net-mode-dependent pair from the
 * lap-count bits), places the car at the start line from that offset and
 * the track's start heading, seats both position mirrors and the "B" chase
 * camera, clears the impulse block, zeroes velocity, and either defaults
 * the heading pair or -- when a start node is registered -- seeds the
 * lap/total/countdown clocks and points the heading pair down the node's
 * own direction.  Finishes by zeroing the grid-race-position bookkeeping,
 * the sticky "behind" flags (net-mode-gated), a run of HUD/timing fields,
 * and re-running the car's table and secondary init passes. */
/* @implements 0x1005E7B0 glide BrCarStartInit_1005E7B0
 * @cpp_kind method
 * @cpp_symbol ?StartInit@Car5E7B0@@QAEXXZ
 *
 * Thiscall, no stack args.  Own TU, own minimal class view (the
 * 0x1006FCE0.cpp / 0x1005C6D0.cpp pattern): only the fields and callees
 * this one method touches are declared.  +0xF8C/F90 the node table,
 * +0xFAC/FA4 the "behind" ints and +0xEA0 the impulse block are the same
 * storage br_carphys.c and 0x1006E5C0/0x10067C30's dossiers already pin;
 * +0x140/+0x144 the lap-count pair 0x1005ACE0/0x10059A80 also read.
 *
 * Not byte-exact: 246/246 instructions (/O2 /Gi, the lane's variant for
 * this file), rows 3+3 register-blind, measured with every relocation
 * resolved to its address (the sweep's masked counts are blind to
 * operand-order swaps -- see docs/VC5-IDIOMS.md).  Source facts, all fixed
 * by the bytes:
 *  - the start-node test is `!= 0` with the node arm first; the grid
 *    position is one statement `DAT_100b2f00 - f140 - 1` before the two zero
 *    stores;
 *  - the impulse block is a struct cleared by its own inline method: as
 *    plain member stores VC5 interleaves them with the SetVel pushes, while
 *    the inline's scope puts them after the pushes and `mov ecx,esi`, as
 *    the original does;
 *  - 0x10077980 / 0x100778AC..E4 / 0x10077904..88 are this TU's CONSTANT
 *    POOL (tu_map evidence): the source wrote literals -- -pi/2, -0.1, 0.5,
 *    3.0, -1.0, 8.0, -0.5, -8.0 -- subtracted as negatives, not extern
 *    floats.  Literals are a different operand kind: with externs VC5
 *    reassociated `s2 * (l10 - 0.5) * 3` into `(s2 * 3) * (l10 - 0.5)`;
 *  - every SetPos product is `(trig * diff) * scale`, both diffs formed
 *    first -- read step by step off the original's x87 sequence;
 *  - the x87 spill slots of the four trig temps are handed out by
 *    reference weight, ties to the later temp: with one use each, s1 and c2
 *    swap slots against the original.  Storing the c2 and s1 products back
 *    into their own temps gives each an extra reference; that brings the
 *    block's instruction multiset to the original's (rows 3+3) but not its
 *    order.
 * RESIDUE, all scheduling:
 *  - the SetPos x87 block (~40 lines): s1/c2 spill slots still swapped
 *    and the fxch/fmul schedule differs.  Dead beyond the sweeps:
 *    declaration order, names, scoping, volatile (per temp, all 16 masks),
 *    inline helpers, double-returning trig prototypes, named argument
 *    temps, arrays, 1..64 preceding functions or declarations, the real
 *    preceding TU (0x1005C6D0 / 0x1005C8B0 / 0x1005E6A0 / 0x1005E780),
 *    dead inits, casts, comma expressions;
 *  - `mov ecx,esi` for 0x1005E6A0 sits after the 14 zero stores in the
 *    original.  Across every byte-exact function in the project VC5 never
 *    leaves that move below a same-block store run.  Dead: volatile fields,
 *    a named `this` copy, fastcall free-function calls, comma/inline object
 *    expressions, duplicated if/else tails, float field types, store
 *    order, an inline member around any sub-run of the stores.
 * Compilers: VC5 RTM, SP3 and VC6 all tried; RTM is closest.
 */
/* @t4-pass 0x1005E7B0 1 2026-09-24 probes 24 bytes 979 insns 246 regions 4 rows 6 census yes  (moved: regions 5->4, insns 244->246.  Impulse-store order x12 -- member order, one pointer, arrays, int stores, loops, a Car-level inline: no movement; a sub-object with its own inline Clear() puts the stores after the pushes and `mov ecx,esi` -- the scope of an inline on a different `this` is the boundary) */
/* @t4-pass 0x1005E7B0 2 2026-09-24 probes 12 bytes 979 insns 246 regions 4 rows 6 census no  (the `mov ecx,esi` before 0x1005E6A0: an inline member around each sub-run of the 16 zero stores x8, goto label, do/while(0), for(;;)/break, named `this` copy for both calls) */
/* @t4-pass 0x1005E7B0 3 2026-09-24 probes 15 bytes 979 insns 246 regions 4 rows 6 census yes  (corpus query at +0x38B: no member holds 3+ of the 12 instructions; the original hoists `mov ecx,esi` over only the last two stores, so the boundary follows +0x1058: the 0x1030..0x1058 run as a sub-object struct with its own Clear() x3 perturbs the mirror copies above; 12 compiler options: /O2 /Gi, /Ox /Gi, /Gi without /GX, /G5, /Ob2, /Gy tie, all others worse) */
/* @t3 0x1005E7B0 2026-09-24 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 979/979 insns 246/246 rows 3+3 regions 4 oracle EQUIVALENT
 * @t3-effort passes 3 zero-movement 2 3
 * Residue is x87 scheduling in the SetPos block and one `mov ecx,esi`
 * placement (see RESIDUE above); A5 EQUIVALENT on 7 real calls over four
 * scripts (quick race, finish, time attack, multiplayer). */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)

/* +0xEA0 impulse block: vector + countdown, reset by its own inline. */
struct Imp5E7B0 {
    float x, y, z;
    int   t;
    void Clear() { x = 0.0f; y = 0.0f; z = 0.0f; t = -180; }
};

class Car5E7B0 {
public:
    char  pad0000[0x30];
    float f30, f34, f38;             /* +0x030 position                 */
    char  pad003C[0x140 - 0x3C];
    int   f140;                      /* +0x140 lap count / grid slot    */
    int   f144;                      /* +0x144 lap count, other net side*/
    char  pad0148[0xE64 - 0x148];
    int   fE64;                      /* +0xE64                          */
    char  pad0E68[0xEA0 - 0xE68];
    Imp5E7B0 imp;                    /* +0xEA0 impulse block            */
    char  pad0EB0[0xF04 - 0xEB0];
    int   fF04;                      /* +0xF04                          */
    char  pad0F08[0xF5C - 0xF08];
    float fF5C, fF60, fF64;          /* +0xF5C position mirror          */
    int   fF68, fF6C, fF70;          /* +0xF68                          */
    float fF74;                      /* +0xF74                          */
    int   fF78;                      /* +0xF78                          */
    char  pad0F7C[0xF80 - 0xF7C];
    float fF80, fF84, fF88;          /* +0xF80 position mirror          */
    int   fF8C;                      /* +0xF8C node table base           */
    int   fF90;                      /* +0xF90 node index                */
    float fF94;                      /* +0xF94 heading dx                */
    float fF98;                      /* +0xF98 heading dz                */
    float fF9C;                      /* +0xF9C                           */
    int   fFA0;                      /* +0xFA0                          */
    int   fFA4;                      /* +0xFA4 "behind" flag            */
    float fFA8x;                     /* +0xFA8                          */
    int   fFAC;                      /* +0xFAC "behind" flag            */
    float fFB0;                      /* +0xFB0 lap clock                */
    char  pad0FB4[0xFE4 - 0xFB4];
    float fFE4;                      /* +0xFE4                          */
    float fFE8;                      /* +0xFE8                          */
    float fFEC;                      /* +0xFEC total clock              */
    float fFF0;                      /* +0xFF0 countdown                */
    float fFF4;                      /* +0xFF4 heading accumulator      */
    int   fFF8;                      /* +0xFF8 grid position - 1        */
    char  pad0FFC[0x1020 - 0xFFC];
    int   f1020;                     /* +0x1020                         */
    char  pad1024[0x1030 - 0x1024];
    int   f1030, f1034, f1038, f103C;
    int   f1040, f1044, f1048, f104C, f1050, f1054, f1058;
    char  pad105C[0x2718 - 0x105C];
    int   f2718;
    char  pad271C[0x294C - 0x271C];
    int   f294C;
    char  pad2950[0x2990 - 0x2950];
    int   f2990;

    void StartInit();                /* 0x1005E7B0 -- defined here       */
    void m_1006FD90();                 /* 0x1006FD90 BrEntReset            */
    void m_1006FCB0(int slot);             /* 0x1006FCB0 grid bind */
    void m_1005E6A0();                 /* 0x1005E6A0 BrCarInitTables       */
    void m_1005E780();                 /* 0x1005E780                       */
    void m_1006F680(float x, float y, float z); /* 0x1006F680 SetPos */
    void m_1006F720(float a);        /* 0x1006F720 SetHeading */
    void m_1006FA10(float x, float y, float z);  /* 0x1006FA10 SetVel */
};

typedef char chk_30 [(unsigned)&((Car5E7B0 *)0)->f30  == 0x30  ? 1 : -1];
typedef char chk_140[(unsigned)&((Car5E7B0 *)0)->f140 == 0x140 ? 1 : -1];
typedef char chk_144[(unsigned)&((Car5E7B0 *)0)->f144 == 0x144 ? 1 : -1];
typedef char chk_e64[(unsigned)&((Car5E7B0 *)0)->fE64 == 0xE64 ? 1 : -1];
typedef char chk_f78[(unsigned)&((Car5E7B0 *)0)->fF78 == 0xF78 ? 1 : -1];
typedef char chk_f80[(unsigned)&((Car5E7B0 *)0)->fF80 == 0xF80 ? 1 : -1];
typedef char chk_fa0[(unsigned)&((Car5E7B0 *)0)->fFA0 == 0xFA0 ? 1 : -1];
typedef char chk_fac[(unsigned)&((Car5E7B0 *)0)->fFAC == 0xFAC ? 1 : -1];
typedef char chk_fb0[(unsigned)&((Car5E7B0 *)0)->fFB0 == 0xFB0 ? 1 : -1];
typedef char chk_fec[(unsigned)&((Car5E7B0 *)0)->fFEC == 0xFEC ? 1 : -1];
typedef char chk_ff0[(unsigned)&((Car5E7B0 *)0)->fFF0 == 0xFF0 ? 1 : -1];
typedef char chk_ff4[(unsigned)&((Car5E7B0 *)0)->fFF4 == 0xFF4 ? 1 : -1];
typedef char chk_ff8[(unsigned)&((Car5E7B0 *)0)->fFF8 == 0xFF8 ? 1 : -1];
typedef char chk_1020[(unsigned)&((Car5E7B0 *)0)->f1020 == 0x1020 ? 1 : -1];
typedef char chk_2718[(unsigned)&((Car5E7B0 *)0)->f2718 == 0x2718 ? 1 : -1];
typedef char chk_294c[(unsigned)&((Car5E7B0 *)0)->f294C == 0x294C ? 1 : -1];
typedef char chk_2990[(unsigned)&((Car5E7B0 *)0)->f2990 == 0x2990 ? 1 : -1];
typedef char chk_f8c[(unsigned)&((Car5E7B0 *)0)->fF8C == 0xF8C ? 1 : -1];
typedef char chk_f94[(unsigned)&((Car5E7B0 *)0)->fF94 == 0xF94 ? 1 : -1];

extern "C" {
extern int   DAT_10226a48;              /* 0x10226A48 net mode            */
extern int   DAT_100b3858;              /* 0x100B3858 entrant count       */
extern int   DAT_100b3014;              /* 0x100B3014 session setting     */
extern int   DAT_104b15e8;              /* 0x104B15E8 stage/lap index     */
extern int   DAT_100b2f00;              /* 0x100B2F00 car count (nEntA)   */
extern int   DAT_106eed48;              /* 0x106EED48 current start node  */
extern int   DAT_100a9360;              /* 0x100A9360 game mode           */
extern float DAT_106eed24;              /* 0x106EED24 start-line heading  */
extern float DAT_106eed18, DAT_106eed1c, DAT_106eed20; /* start line origin */
extern char *PTR_PTR_100bcab0[];        /* per-session-setting grid table */
float BrCosF(float x);                  /* 0x100023E0 */
float BrSinF(float x);                  /* 0x10002560 */
void  BrVec3Direction(float *pOut, const float *pFrom, const float *pTo); /* 0x10034420 */
void  __fastcall BrCamFrameInitB(unsigned char *p);   /* 0x10002310      */
}

void Car5E7B0::StartInit()
{
    float local_14, local_10;
    float c1, s1, c2, s2;
    short sVar1;

    m_1006FD90();

    if (DAT_100a9360 == 2 || DAT_100a9360 == 4 ||
        (DAT_100a9360 == 3 && DAT_100b3858 == 1) || DAT_100a9360 == 0) {
        m_1006FCB0(f140);
        local_14 = 0.0f;
        local_10 = 0.5f;
    } else {
        m_1006FCB0(f140);
        if (DAT_10226a48 != 0) {
            local_14 = (float)(f144 >> 1);
            local_10 = (float)(~f144 & 1);
        } else {
            local_14 = (float)(f140 >> 1);
            local_10 = (float)(~f140 & 1);
        }
    }

    c1 = BrCosF(DAT_106eed24);
    s1 = BrSinF(DAT_106eed24);
    c2 = BrCosF(DAT_106eed24 - (-1.5707964f));
    s2 = BrSinF(DAT_106eed24 - (-1.5707964f));

    c2 = c2 * (local_10 - 0.5f) * 3.0f;
    s1 = s1 * (local_14 - (-1.0f)) * 8.0f;
    m_1006F680((DAT_106eed18 - c2) -
               c1 * (local_14 - (-1.0f)) * 8.0f,
           (DAT_106eed1c - s2 * (local_10 - 0.5f) * 3.0f) -
               s1,
           DAT_106eed20 - (-0.1f));

    fFF4 = (local_14 - (-0.5f)) * (-8.0f);

    m_1006F720(DAT_106eed24);

    fF80 = f30;
    fF84 = f34;
    fF88 = f38;
    fF5C = f30;
    fF60 = f34;
    fF64 = f38;

    BrCamFrameInitB((unsigned char *)this);

    imp.Clear();

    m_1006FA10(0.0f, 0.0f, 0.0f);

    if (DAT_106eed48 != 0) {
        fFB0 = 0.0f;
        fFEC = 0.0f;
        fFE4 = 0.0f;
        sVar1 = (short)DAT_104b15e8 - 1;
        if (sVar1 > 2 || sVar1 < 0)
            sVar1 = 0;
        fFF0 = *(float *)(PTR_PTR_100bcab0[DAT_100b3014] +
                           (fE64 * 3 + sVar1) * 0x1c + 0x44);
        fF8C = DAT_106eed48;
        fF90 = 0;
        BrVec3Direction(&fF94, (const float *)(DAT_106eed48 + 0x4c),
                        (const float *)(DAT_106eed48 + 0x74));
    } else {
        fF94 = 1.0f;
        fF98 = 0.0f;
        fF9C = 0.0f;
    }

    fFF8 = DAT_100b2f00 - f140 - 1;
    fFA8x = 0.0f;
    fFE8 = 0.0f;
    fFA0 = 0;

    if (f140 < DAT_100b3858 || DAT_100a9360 == 1 || DAT_100a9360 == 2 ||
        DAT_100a9360 == 4 || DAT_100a9360 == 6) {
        fFAC = -1;
        fFA4 = -1;
    } else {
        fFAC = 0;
        fFA4 = 0;
    }

    f294C = 0;
    f2990 = 0;
    f2718 = 0;
    f1034 = 0;
    f1030 = 0;
    f1038 = 0;
    f103C = 0;
    f1040 = 0;
    f1044 = 0;
    f1048 = 0;
    f104C = 0;
    f1050 = 0;
    f1054 = 0;
    f1058 = 0;
    f1020 = 0;
    fF04 = 0;

    m_1005E6A0();
    m_1005E780();

    fF78 = 0;
    fF68 = 0;
    fF6C = 0;
    fF70 = 0;
    fF74 = 1.0f;
}

#endif /* BR_MATCHING_BUILD */
