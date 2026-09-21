/* br_carctor.c -- 0x1005BCC0, the car/entity constructor.
 *
 * thiscall on the car.  Four wheel rigid bodies, two contact chains, the
 * live BrRbState at car+0x1DC.  Matching build only.
 *
 * RESIDUE 2026-09-12: 1872/1909 B, 466/471 insns, REGNORM 7+12, A4 clean.
 * First size change is the 1.0f stores (orig rematerialises imm32 at
 * +0x60c/+0xa24 while a 0x3f800000 register is live) and five `lea`
 * of the wheel-body pointers at +0x168 -- orig re-leas after the last
 * BuildMatrix because those addresses were passed in eax (caller-saved);
 * ours keep them in a callee-saved register from InitInertia.  Record
 * pointer is re-derived per field (the BrInputPoll load idiom).
 */
#ifdef BR_MATCHING_BUILD

#include <stdint.h>
#include "br_match.h"

extern float _DAT_10077864;
extern float _DAT_1007788c;
extern float _DAT_10077890;

void BrRbInitInertia(void *pB);
void BrRbBuildMatrix(void *pM, void *pS);
void BrX100746E0(unsigned int *pDst,
                 unsigned int a2, unsigned int a3, unsigned int a4,
                 unsigned int a5, unsigned int a6, unsigned int a7,
                 unsigned int a8);
int  BrPodNop();

#define DW(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))
#define FL(p, o) (*(float *)((uint8_t *)(p) + (o)))
#define BY(p, o) (*(uint8_t *)((uint8_t *)(p) + (o)))

/* 2026-09-13: the 1.0f web. The original holds 0x3f800000 in ebp for the
 * +0x1f4 and +0x400 stores only, then gives ebp to pRF (= p + 0x800) and
 * writes +0x818 / +0x60c / +0xa24 and the four BrX100746E0 pushes as
 * immediates; ours keeps the constant in edi for the whole function and pRF
 * never gets a callee-saved register. DEAD: typing the late stores (or all
 * seven) as `FL(p, o) = 1.0f`, and `pRF[6] = 1.0f` -- VC5 folds a float
 * constant store into the live int register web every time (1235 each). */
/* WHAT IT DOES: initialise one car's physics bodies, wheel states and the
 * two contact-point chains.  Four rigid bodies (chassis plus three more
 * wheel/axle slots) get their inertia filled and a pose matrix built from
 * a default state; the car-data record at +0x29C4 supplies the wheel
 * positions.  Two linked lists of contact nodes are wired up, four debug
 * prints dump the wheel points, and a couple of mode bytes are cleared. */
/* @t4-pass 0x1005BCC0 1 2026-09-20 probes 12 bytes 1871 insns 466 regions 17 rows 31 census yes  (1.0f web: k1 int-const naming folds like the imm; DEAD with the 09-13 float-typing attempts) */
/* @t4-pass 0x1005BCC0 2 2026-09-20 probes 10 bytes 1871 insns 466 regions 17 rows 31 census no   (pRF[] indexing of the +0x800 block does not pin pRF callee-saved; wheel-body re-lea residue unmoved) */
/* @t3 0x1005BCC0 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1871/1909 insns 466/471 rows 18+13 regions 17 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register allocation only: the 1.0f web (orig parks 0x3f800000 in
 * ebp then recycles it as pRF, ours keeps it live and re-materialises the late
 * stores as immediates) and the five wheel-body pointer re-leas at +0x168
 * (orig re-computes p+0x370.. after the calls clobber caller-saved eax; ours
 * keeps them callee-saved).  A5 oracle EQUIVALENT is the completeness proof
 * (rule 12).  Do not reopen before the end-grind. */
/* @implements 0x1005BCC0 glide BrSub10062C50 */
void BR_THISCALL1 BrSub10062C50(void *pCar)
{
    uint8_t *p = (uint8_t *)pCar;
    float *pLF;
    float *pRF;

    DW(p, 0xe84) = 1;
    DW(p, 0xe20) = 0;
    DW(p, 0xe70) = 0;
    DW(p, 0xe24) = 0;
    if (DW(p, 0x29c4) != 0) {
    uint32_t k2 = 0x40000000;           /* 2.0f, orig edi */

    DW(p, 0x180) = 1;
    DW(p, 0x190) = 0x447a0000;          /* 1000.0f */
    DW(p, 0x184) = 0x40600000;          /* 3.5f */
    DW(p, 0x188) = k2;
    DW(p, 0x18c) = 0x3fc00000;          /* 1.5f */
    BrRbInitInertia(p + 0x164);

    DW(p, 0x1dc) = 0;
    DW(p, 0x1e0) = 0;
    DW(p, 0x1e4) = k2;
    DW(p, 0x1e8) = 0;
    DW(p, 0x1ec) = 0;
    DW(p, 0x1f0) = 0;
    DW(p, 0x1f4) = 0x3f800000;
    DW(p, 0x1f8) = 0;
    DW(p, 0x1fc) = 0;
    DW(p, 0x200) = 0;
    DW(p, 0x204) = 0;
    DW(p, 0x208) = 0;
    DW(p, 0x20c) = 0;
    BrRbBuildMatrix(p + 0x220, p + 0x1dc);

    FL(p, 0x31c) = (_DAT_10077864 - (float)(int32_t)DW(p, 0xe94) * _DAT_1007788c)
                   * _DAT_10077890;
    DW(p, 0x320) = 0xc53b8000;          /* -3000.0f */
    DW(p, 0x548) = 0;
    DW(p, 0x50c) = 0;
    DW(p, 0x534) = 0;
    DW(p, 0x38c) = 2;
    DW(p, 0x39c) = 0;
    DW(p, 0x390) = 0;
    DW(p, 0x394) = 0;
    DW(p, 0x398) = 0;
    BrRbInitInertia(p + 0x370);

    pLF = (float *)(p + 0x3e8);
    *pLF = *(float *)(DW(p, 0x29c4) + 0x80f8);
    DW(p, 0x3ec) = *(uint32_t *)(DW(p, 0x29c4) + 0x80fc);
    DW(p, 0x3f0) = 0xbdcccccd;          /* -0.1f */
    DW(p, 0x3f4) = 0;
    DW(p, 0x3f8) = 0;
    DW(p, 0x3fc) = 0;
    DW(p, 0x400) = 0x3f800000;
    DW(p, 0x404) = 0;
    DW(p, 0x408) = 0;
    DW(p, 0x40c) = 0;
    DW(p, 0x410) = 0;
    DW(p, 0x414) = 0;
    DW(p, 0x418) = 0;
    BrRbBuildMatrix(p + 0x42c, p + 0x3e8);

    DW(p, 0x960) = 0;
    DW(p, 0x924) = 0;
    DW(p, 0x94c) = 0;
    DW(p, 0x7a4) = 2;
    DW(p, 0x7b4) = 0;
    DW(p, 0x7a8) = 0;
    DW(p, 0x7ac) = 0;
    DW(p, 0x7b0) = 0;
    BrRbInitInertia(p + 0x788);

    pRF = (float *)(p + 0x800);
    *pRF = *(float *)(DW(p, 0x29c4) + 0x80f8);
    FL(p, 0x804) = -*(float *)(DW(p, 0x29c4) + 0x80fc);
    DW(p, 0x808) = 0xbdcccccd;
    DW(p, 0x80c) = 0;
    DW(p, 0x810) = 0;
    DW(p, 0x814) = 0;
    DW(p, 0x818) = 0x3f800000;
    DW(p, 0x81c) = 0;
    DW(p, 0x820) = 0;
    DW(p, 0x824) = 0;
    DW(p, 0x828) = 0;
    DW(p, 0x82c) = 0;
    DW(p, 0x830) = 0;
    BrRbBuildMatrix(p + 0x844, p + 0x800);

    DW(p, 0x754) = 0;
    DW(p, 0x718) = 0;
    DW(p, 0x740) = 0;
    DW(p, 0x598) = 2;
    DW(p, 0x5a8) = 0;
    DW(p, 0x59c) = 0;
    DW(p, 0x5a0) = 0;
    DW(p, 0x5a4) = 0;
    BrRbInitInertia(p + 0x57c);

    DW(p, 0x5f4) = *(uint32_t *)(DW(p, 0x29c4) + 0x80ec);
    DW(p, 0x5f8) = *(uint32_t *)(DW(p, 0x29c4) + 0x80f0);
    DW(p, 0x5fc) = 0xbdcccccd;
    DW(p, 0x600) = 0;
    DW(p, 0x604) = 0;
    DW(p, 0x608) = 0;
    DW(p, 0x60c) = 0x3f800000;
    DW(p, 0x610) = 0;
    DW(p, 0x614) = 0;
    DW(p, 0x618) = 0;
    DW(p, 0x61c) = 0;
    DW(p, 0x620) = 0;
    DW(p, 0x624) = 0;
    BrRbBuildMatrix(p + 0x638, p + 0x5f4);

    DW(p, 0xb6c) = 0;
    DW(p, 0xb30) = 0;
    DW(p, 0xb58) = 0;
    DW(p, 0x9b0) = 2;
    DW(p, 0x9c0) = 0;
    DW(p, 0x9b4) = 0;
    DW(p, 0x9b8) = 0;
    DW(p, 0x9bc) = 0;
    BrRbInitInertia(p + 0x994);

    DW(p, 0xa0c) = *(uint32_t *)(DW(p, 0x29c4) + 0x80ec);
    FL(p, 0xa10) = -*(float *)(DW(p, 0x29c4) + 0x80f0);
    DW(p, 0xa14) = 0xbdcccccd;
    DW(p, 0xa18) = 0;
    DW(p, 0xa1c) = 0;
    DW(p, 0xa20) = 0;
    DW(p, 0xa24) = 0x3f800000;
    DW(p, 0xa28) = 0;
    DW(p, 0xa2c) = 0;
    DW(p, 0xa30) = 0;
    DW(p, 0xa34) = 0;
    DW(p, 0xa38) = 0;
    DW(p, 0xa3c) = 0;
    BrRbBuildMatrix(p + 0xa50, p + 0xa0c);

    {
        uint8_t *pA = p + 0x370;
        uint8_t *pB = p + 0x788;
        uint8_t *pC = p + 0x57c;
        uint8_t *pD = p + 0x994;
        DW(p, 0x168) = (uint32_t)pA;
        DW(p, 0x16c) = (uint32_t)pB;
        DW(p, 0x170) = (uint32_t)pC;
        DW(p, 0x174) = (uint32_t)pD;
    }

    BrX100746E0((unsigned int *)(p + 0xba0), 0, 0, 0, 0xbfc00000u, 0xbf800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xbe0), 0, 0, 0, 0xbfc00000u, 0x3f800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xbc0), 0, 0, 0, 0x3fc00000u, 0xbf800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xc00), 0, 0, 0, 0x3fc00000u, 0x3f800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xca0), 0, 0, 0xc6716b00u, 0, 0, 0, 0);
    BrX100746E0((unsigned int *)(p + 0xcc0), 0, 0, 0xc2ce028fu, 0, 0, 0, 0);
    BrX100746E0((unsigned int *)(p + 0xce0), 0, 0, 0xc2ce028fu, 0, 0, 0, 0);

    DW(p, 0xba0) = (uint32_t)(p + 0xbe0);
    DW(p, 0xbe0) = (uint32_t)(p + 0xbc0);
    DW(p, 0xbc0) = (uint32_t)(p + 0xc00);
    DW(p, 0x17c) = (uint32_t)(p + 0xba0);
    DW(p, 0xc00) = (uint32_t)(p + 0xca0);
    DW(p, 0xca0) = 0;

    BrX100746E0((unsigned int *)(p + 0xd20), 0, 0, 0, 0, 0, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xd60), 0, 0, 0, 0, 0, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xd40), 0, 0, 0, 0, 0, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xd80), 0, 0, 0, 0, 0, 0, 1);

    DW(p, 0x388) = (uint32_t)(p + 0xd20);
    DW(p, 0x594) = (uint32_t)(p + 0xd40);
    DW(p, 0x9ac) = (uint32_t)(p + 0xd80);
    DW(p, 0xd20) = (uint32_t)(p + 0xce0);
    DW(p, 0xd40) = (uint32_t)(p + 0xcc0);
    DW(p, 0xd80) = (uint32_t)(p + 0xcc0);
    DW(p, 0xcc0) = 0;
    DW(p, 0x7a0) = (uint32_t)(p + 0xd60);
    DW(p, 0xd60) = (uint32_t)(p + 0xce0);
    DW(p, 0xce0) = 0;

    BrX100746E0((unsigned int *)(p + 0xc20), 0, 0, 0, 0xbfc00000u, 0xbf800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xc60), 0, 0, 0, 0xbfc00000u, 0x3f800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xc40), 0, 0, 0, 0x3fc00000u, 0xbf800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xc80), 0, 0, 0, 0x3fc00000u, 0x3f800000u, 0, 1);
    BrX100746E0((unsigned int *)(p + 0xd00), 0, 0, 0, 0, 0, 0, 0);

    DW(p, 0xc20) = (uint32_t)(p + 0xc60);
    DW(p, 0xc60) = (uint32_t)(p + 0xc40);
    DW(p, 0xc40) = (uint32_t)(p + 0xc80);
    DW(p, 0xc80) = (uint32_t)(p + 0xd00);
    DW(p, 0xd00) = 0;

    BY(p, 0xe81) = 0;
    DW(p, 0xe7c) = 0;
    DW(p, 0xe74) = 0;
    BY(p, 0x361) = BY(p, 0xe90);

    BrPodNop("LF = %10.6f, %10.6f, %10.6f\n",
             (double)pLF[0], (double)FL(p, 0x3ec), (double)FL(p, 0x3f0));
    BrPodNop("RF = %10.6f, %10.6f, %10.6f\n",
             (double)pRF[0], (double)FL(p, 0x804), (double)FL(p, 0x808));
    BrPodNop("LR = %10.6f, %10.6f, %10.6f\n",
             (double)FL(p, 0x5f4), (double)FL(p, 0x5f8), (double)FL(p, 0x5fc));
    BrPodNop("RR = %10.6f, %10.6f, %10.6f\n",
             (double)FL(p, 0xa0c), (double)FL(p, 0xa10), (double)FL(p, 0xa14));
    }

    BY(p, 0xe80) = 0;
    BY(p, 0xe78) = 0;
}

#endif /* BR_MATCHING_BUILD */
